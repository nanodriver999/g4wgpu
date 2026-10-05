# g4wgpu 구현 계획

## 목표

기존 Geant4 C++ 애플리케이션을 유지하면서 e-/e+/gamma 중심의 계산 집약 부분을
WebGPU compute backend로 단계적으로 가속한다.

기본 native runtime은 `wgpu-native`를 사용하되 프로젝트의 상위 API는
가능한 한 표준 `webgpu.h`에 맞춘다.

## 비목표

초기 단계에서는 다음을 하지 않는다.

- Geant4 전체 재작성
- Rust로 application rewrite
- `G4SteppingManager` 전체 GPU 이식
- 모든 physics process 지원
- CPU 구현 삭제
- bitwise 동일성 요구

## PR 전략

각 PR은 하나의 독립적인 검증 목표를 가진다.
후속 PR은 이전 PR이 리뷰/병합된 상태를 base로 한다.

### PR 1 — 분석 및 구현계획

산출물:
- `docs/geant4-gpu-analysis.md`
- `docs/implementation-plan.md`
- 프로젝트 README 정리

검증:
- 대상 Geant4 commit SHA 기록
- GPU/CPU 경계 명시
- 라이선스 위험 명시
- 실제 구현 단계 정의

### PR 2 — C++ core와 backend abstraction

산출물:
- CMake 프로젝트
- `ComputeBackend` interface
- `TrackBatch` POD/SoA 구조
- CPU reference backend
- 단위 테스트

목표:
WebGPU가 없어도 core 데이터 모델과 backend 경계를 빌드/테스트 가능하게 한다.

Acceptance:
- C++17 이상에서 빌드
- CPU smoke test 통과
- 외부 GPU dependency 없이 기본 build 가능

### PR 3 — WebGPU runtime adapter

산출물:
- `WebGpuBackend`
- `webgpu.h` 기반 instance/adapter/device 초기화
- buffer upload/download
- 최초 WGSL compute shader

초기 shader:
`y[i] = a*x[i] + y[i]` 형태의 단순 vector kernel.

목적:
physics code를 넣기 전에 runtime, buffer, dispatch, synchronization 경로를 검증한다.

Acceptance:
- `G4WGPU_ENABLE_WEBGPU=OFF` 기본 build 유지
- WebGPU enabled build에서 adapter/device 생성
- CPU와 GPU vector 결과 비교

### PR 4 — Batch/RNG infrastructure

산출물:
- track SoA batch
- GPU deterministic RNG prototype
- seed/counter mapping
- batch compaction/result status

Acceptance:
- 동일 seed/동일 batch order에서 재현 가능
- statistical sanity test
- GPU buffer 왕복 없이 여러 kernel 연속 실행 가능

### PR 5 — 첫 physics kernel: Klein–Nishina

대상:
`G4KleinNishinaCompton::SampleSecondaries`의 계산 모델을 참고하여
독립적인 GPU kernel을 작성한다.

구현:
- CPU reference implementation
- WGSL implementation
- rejection sampling
- output energy/direction

Acceptance:
- energy/momentum invariant 검증
- CPU/GPU histogram 비교
- 여러 seed에서 통계적 동등성 확인

주의:
Geant4 구현을 그대로 복사하지 않고 알고리즘/논문을 기준으로 독립 구현한다.

### PR 6 — Geant4 custom tracking manager integration

산출물:
- `G4WgpuTrackingManager : G4VTrackingManager`
- `HandOverOneTrack()`
- `FlushEvent()`
- gamma/electron/positron 선택적 registration
- CPU fallback

처음에는 실제 전체 track을 GPU에서 끝까지 추적하지 않는다.

구조:
```text
HandOverOneTrack
   ↓
eligibility check
   ├─ unsupported → normal CPU path
   └─ supported   → batch
                     ↓
                  FlushEvent
                     ↓
               selected GPU operation
```

Acceptance:
- 기존 Geant4 application에 plugin 방식으로 연결
- unsupported track은 CPU fallback
- event lifecycle과 secondary stack 보존

### PR 7 — Geometry primitive kernels

우선 지원:
- Box
- Sphere/Orb
- Tubs

대상 연산:
- DistanceToIn
- DistanceToOut
- safety

Acceptance:
- CPU Geant4 geometry 결과와 tolerance 기반 비교
- boundary/near-boundary regression suite

### PR 8 — Batched navigation experiment

`G4NormalNavigation` 전체 복사 대신
flattened geometry descriptor와 primitive kernel을 조합한다.

Acceptance:
- 단순 detector geometry에서 next boundary 결과 비교
- GPU batch size별 성능 측정

### PR 9 — e-/e+/gamma EM pipeline 확대

후보:
- Moller/Bhabha sampling
- Bethe-Bloch secondary sampling
- selected bremsstrahlung
- multiple scattering 일부

각 모델은 별도 feature flag로 추가하고 CPU fallback을 유지한다.

## 소스 구조 목표

```text
g4wgpu/
├ CMakeLists.txt
├ cmake/
├ include/g4wgpu/
│  ├ ComputeBackend.hh
│  ├ TrackBatch.hh
│  ├ CpuBackend.hh
│  └ WebGpuBackend.hh
├ src/
│  ├ core/
│  ├ cpu/
│  ├ webgpu/
│  └ geant4/
├ shaders/
│  ├ smoke/
│  ├ rng/
│  ├ em/
│  └ geometry/
├ tests/
│  ├ unit/
│  ├ numerical/
│  └ integration/
└ docs/
```

## 성능 원칙

GPU 이식 여부는 함수가 GPU에서 실행 가능한지가 아니라
전체 실행시간 감소 여부로 결정한다.

다음 식으로 평가한다.

```text
net_gain =
CPU_kernel_time
- GPU_dispatch
- upload
- download
- GPU_kernel_time
```

가능한 경우 데이터는 GPU에 오래 유지한다.

나쁜 구조:
```text
CPU → GPU A → CPU → GPU B → CPU
```

목표:
```text
CPU → GPU
       ├ A
       ├ B
       ├ C
       └ D
     → CPU
```

## 검토 규칙

각 PR에서 반드시 확인한다.

1. CPU fallback이 유지되는가
2. Geant4 public API 경계를 불필요하게 침범하지 않는가
3. object pointer를 GPU buffer에 직접 저장하지 않는가
4. FP32/FP64 차이를 숨기지 않는가
5. RNG reproducibility가 깨지지 않는가
6. benchmark 없이 성능 향상을 주장하지 않는가
7. Geant4 라이선스 코드를 무단 복제하지 않는가
8. WebGPU-specific extension을 사용하는 경우 adapter 계층에 격리했는가

## 성공 기준

1차 연구 성공 기준은 Geant4 전체 GPU화가 아니다.

다음 결과를 목표로 한다.

- 기존 C++ Geant4 프로그램 유지
- 특정 physics/geometry kernel의 WebGPU 실행
- NVIDIA/AMD/Intel/Apple 계열에서 동일 코드 경로 사용 가능성 검증
- CPU reference와 정확도/통계 비교
- 실제 end-to-end speedup 측정
- runtime을 wgpu-native 중심으로 사용하되 WebGPU C API 경계를 유지
