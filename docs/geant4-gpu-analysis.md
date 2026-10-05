# Geant4 → WebGPU 분석 결과

기준 소스:
- `nanodriver999/geant4@f3d5293d384757b8a228a099898b2b87cfa4023c`
- Geant4 11.5.0.beta import
- 대상 프로젝트: `nanodriver999/g4wgpu`

## 1. 결론

Geant4 전체를 WebGPU/WGSL로 재작성하지 않는다.

현재 Geant4의 기본 추적 흐름은 다음과 같다.

```text
G4RunManager / G4EventManager
        ↓
G4TrackingManager::ProcessOneTrack
        ↓
G4SteppingManager::Stepping
        ↓
 ┌──────────────┬────────────────┐
 physics        geometry         secondary handling
 processes      navigation
```

이 흐름은 한 개의 `G4Track` 상태를 step-by-step으로 변경하는 stateful orchestration이므로
`G4TrackingManager` 또는 `G4SteppingManager` 자체를 통째로 GPU shader로 옮기는 것은 1차 목표로 삼지 않는다.

대신 Geant4가 제공하는 `G4VTrackingManager` 확장점을 이용하여 동일 종류의 track을 모은 뒤,
GPU에 적합한 계산만 batch 단위로 실행하는 구조를 사용한다.

```text
G4EventManager
   ↓
G4VTrackingManager
   ├─ HandOverOneTrack(track)
   │      └─ batch queue
   │
   └─ FlushEvent()
          ↓
      G4WgpuTrackingManager
          ↓
     TrackBatch (SoA)
          ↓
     WebGPU Backend
          ↓
       WGSL kernels
```

## 2. 3계층 분류

### 계층 1 — Orchestration / Application Logic

GPU로 옮기지 않는다.

주요 대상:
- `source/run/src/G4RunManager.cc`
- `source/run/src/G4WorkerRunManager.cc`
- `source/event/src/G4EventManager.cc`
- `source/tracking/src/G4TrackingManager.cc`
- `source/tracking/src/G4SteppingManager.cc`의 process dispatch / track status / secondary 관리 부분

근거:
- 객체 수명과 포인터 관계가 복잡함
- virtual dispatch와 사용자 hook이 많음
- 각 step에서 process 선택 및 track 상태 변경이 발생함
- I/O, sensitive detector, trajectory, user action 등 GPU와 무관한 작업이 결합되어 있음

특히 `G4SteppingManager::DefinePhysicalStepLength()`,
`InvokeAlongStepDoItProcs()`, `InvokePostStepDoItProcs()`는 수치 계산 자체보다
여러 process를 조합하는 스케줄링 역할이 강하다.

**판정: C++ 유지**

---

### 계층 2 — Numerical Algorithms

GPU backend로 호출 가능한 단위로 분리할 후보이다.

#### A. Geometry navigation

주요 파일:
- `source/geometry/navigation/src/G4Navigator.cc`
- `source/geometry/navigation/src/G4NormalNavigation.cc`
- `source/geometry/navigation/src/G4VoxelNavigation.cc`
- `source/geometry/solids/CSG/src/G4Box.cc`
- `G4Tubs.cc`, `G4Sphere.cc`, `G4Orb.cc`, `G4Cons.cc` 등

`G4NormalNavigation::ComputeStep()`는 daughter volume을 순회하면서
`DistanceToIn`, `DistanceToOut`을 반복 계산한다.

이 계산은 개별 track 기준으로는 branch가 많지만, 다수 track을 동일 geometry에 대해 처리하면
GPU batching 후보가 된다.

**판정: 2차 GPU 후보**

#### B. Electromagnetic physics

주요 파일:
- `G4KleinNishinaCompton.cc`
- `G4MollerBhabhaModel.cc`
- `G4BetheBlochModel.cc`
- `G4UrbanMscModel.cc`
- 기타 `source/processes/electromagnetic/*`

특징:
- 산술 집약적
- 난수 기반 sampling이 많음
- rejection sampling loop 존재
- 개별 track 간 독립성이 높음

**판정: 가장 우선적인 GPU 후보**

#### C. Multiple scattering / transport correction

`G4UrbanMscModel::ComputeTruePathLengthLimit()`는 수치 계산량이 있지만
geometry safety와 track-local 상태에 강하게 연결된다.

**판정: 단계적 이식 후보. 초기 MVP에서는 CPU 유지 후 GPU geometry backend와 함께 검토**

---

### 계층 3 — Hot Computational Kernels

정적 코드 분석 기준 우선 후보:

| Priority | Kernel family | 대표 코드 | GPU 적합성 | 위험 |
|---|---|---|---|---|
| P0 | EM secondary sampling | Klein-Nishina Compton | 높음 | RNG, rejection divergence |
| P0 | Ionisation secondary sampling | Moller/Bhabha, Bethe-Bloch | 높음 | FP64, RNG |
| P1 | primitive solid distance | Box/Sphere/Tubs DistanceToIn/Out | 높음 | geometry tolerance |
| P1 | daughter intersection batches | G4NormalNavigation | 중~높음 | irregular daughter counts |
| P2 | voxel navigation | G4VoxelNavigation | 중간 | pointer-heavy hierarchy |
| P2 | multiple scattering step limit | G4UrbanMscModel | 중간 | stateful + geometry coupling |
| P3 | process orchestration | G4SteppingManager | 낮음 | virtual dispatch/state machine |

실제 runtime share는 프로파일링 전에는 확정하지 않는다.

## 3. 중요한 Geant4 연동점

Geant4 11.x에는 `G4VTrackingManager`가 존재하며 다음 동작을 공식적으로 허용한다.

- `HandOverOneTrack(G4Track*)`: 즉시 처리하거나 나중으로 미룰 수 있음
- `FlushEvent()`: event 내에서 지연된 track을 일괄 처리할 수 있음

`source/event/src/G4EventManager.cc` 역시 custom tracking manager가 track 처리를 지연할 수 있다는 전제로 구현되어 있다.

또한 공식 예제 `examples/extended/runAndEvent/RE07`에는
`SpecializedTrackingManager`가 track을 buffer에 저장했다가 `FlushEvent()`에서 처리하는 패턴이 이미 존재한다.

따라서 g4wgpu는 Geant4 core를 invasive하게 수정하기보다
**custom tracking manager plugin/extension**으로 시작한다.

## 4. 제안 아키텍처

```text
Geant4 C++ application
        │
        ▼
G4VTrackingManager
        │
        ▼
G4WgpuTrackingManager
        │
        ├─ TrackBatchBuilder
        │      └─ G4Track → POD/SoA
        │
        ├─ CpuReferenceBackend
        │
        └─ WebGpuBackend
                │
                ▼
             webgpu.h
                │
             wgpu-native
                │
          Vulkan / Metal / DX12
                │
                ▼
           WGSL compute kernels
```

`webgpu.h` 공통 API를 우선 사용하고 wgpu-native 전용 확장은 별도 adapter 계층에 격리한다.

## 5. 데이터 구조

Geant4 object를 GPU buffer에 그대로 복사하지 않는다.

GPU용 DTO/SoA를 정의한다.

예:

```text
TrackBatch
 ├ position_x[]
 ├ position_y[]
 ├ position_z[]
 ├ direction_x[]
 ├ direction_y[]
 ├ direction_z[]
 ├ kinetic_energy[]
 ├ particle_id[]
 ├ material_id[]
 ├ rng_counter[]
 └ result/status[]
```

이유:
- `G4Track`은 GPU에 적합한 trivially-copyable 구조가 아님
- pointer/object graph를 storage buffer에 직접 표현하지 않음
- coalesced memory access를 가능하게 함
- CPU reference backend와 GPU backend가 같은 입력 형태를 공유할 수 있음

## 6. FP64 위험

Geant4는 `G4double`을 광범위하게 사용한다.

따라서 초기 연구에서 반드시 다음 세 모드를 구분한다.

1. CPU reference: 기존 `double`
2. WebGPU portable: `f32`
3. native extension 가능 시: 선택적 high-precision 경로

GPU 구현 성공의 기준을 bitwise equality로 두지 않는다.
각 physics kernel별로 허용 오차와 통계적 분포 동등성을 정의해야 한다.

Monte Carlo sampling은 개별 event의 완전 동일성보다
다음 검증이 더 중요할 수 있다.

- 평균/분산
- histogram
- KS test 또는 적절한 분포 비교
- 에너지 보존
- secondary multiplicity
- dose / range / attenuation 등 도메인 관측량

## 7. RNG

CLHEP RNG 상태를 GPU에서 직접 공유하지 않는다.

GPU backend에는 counter-based RNG를 별도로 도입하는 방향을 검토한다.

필수 조건:
- track별 독립 stream
- deterministic seed mapping
- reproducible batch scheduling
- CPU/GPU 결과 비교를 위한 seed 기록

RNG 구현은 첫 compute smoke test가 끝난 후 별도 PR로 진행한다.

## 8. 프로파일링 계획

GPU 포팅 전 CPU baseline을 확보한다.

최소 측정 단위:
- event
- particle type
- tracking
- geometry navigation
- EM process
- selected physics model

실측 전에는 특정 kernel이 전체 실행시간의 몇 %라고 주장하지 않는다.

## 9. 라이선스 경계

`g4wgpu`는 현재 MIT template에서 시작했지만 Geant4 소스 자체는 Geant4 Software License를 따른다.

따라서:
- Geant4 원본 구현을 대량 복사하지 않는다.
- 가능한 한 public API를 통한 extension/plugin 방식으로 구현한다.
- 물리식/알고리즘을 독립적으로 WGSL 구현할 때 출처와 검증 근거를 문서화한다.
- Geant4 코드에서 직접 파생된 부분이 생기면 파일별 라이선스 검토를 수행한다.

## 10. 첫 구현 대상

첫 실제 physics kernel은 복잡한 전체 transport가 아니라 아래 순서로 진행한다.

1. WebGPU runtime smoke compute
2. TrackBatch SoA upload/download
3. 단순 독립 수치 kernel
4. Klein-Nishina sampling prototype
5. CPU reference와 통계 검증
6. custom tracking manager batching
7. geometry primitive distance
8. 점진적인 e-/e+/gamma EM pipeline

이 순서는 WebGPU runtime 문제와 physics correctness 문제를 분리하기 위한 것이다.
