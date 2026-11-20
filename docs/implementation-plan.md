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


## GitHub Actions 기반 후속 개발 로드맵 (2026-11-20)

기준: `main@17b2825bc9fdae79696cb179df853c139af34760`의 Box/Orb/Tubs,
Gamma, Geant4 Shadow, fused Box navigation 및 파이프라인 재사용이 구현된 상태.
아래 #81–#93은 **예상 PR 순서**로, 실제 PR 번호가 바뀌면 제목과 완료 기준을
우선한다. 별도 물리 GPU가 없는 현재 GitHub-hosted Ubuntu 24.04 실행 환경에서는
**Mesa software Vulkan을 통한 WGSL 실행/정확도**만 합격 기준으로 사용한다.
Software Vulkan 시간은 진단만 가능하며 실제 GPU의 속도 향상을 입증하지 않는다.

### 모든 PR 공통 병합 게이트

- 최신 `main`과 열린 PR부터 확인하여 중복 작업을 피한다.
- `cpu`: CPU f64 기준, RNG/수치, fallback, 벤치마크 CSV 계약 검증.
- `webgpu-compile-link`: 고정 wgpu-native + Mesa Vulkan에서 **실제 WGSL 실행**,
  raw GPU 결과와 독립 CPU f64 기준 비교, 버퍼 수명과 상태 검증.
- `geant4-compile-link`: 고정 Geant4의 물리·Geometry·Tracking 기준 결과 검증.
- `geant4-webgpu-shadow`: Geant4 + WebGPU 통합, CPU fallback 및 상태 보존.
  신규 통합 테스트는 파일뿐 아니라 해당 CI 명령에도 반드시 연결한다.
- **정확한 PR head SHA**에서 4개 Job이 모두 성공하고 mergeable=true이며
  diff 검토가 끝난 경우에만 squash merge한다. 실패 시 로그 분석 후 수정/재검증.
- wall-time, GPU/CPU ratio, 리소스 사용시간은 artifact에 기록하지만 CI의
  고정 성능 임계값으로 사용하지 않는다. 정확도/카운터/CSV 규격은 강제할 수 있다.
- 기본 Geant4 Navigator·CPU fallback, track/secondary, RNG, 물리 정확도,
  기본 비활성 feature flag를 유지한다.

### Phase A — Runtime 관측과 메모리 재사용

| 예정 PR | 구현 목표 | GitHub Actions 검증 조건 |
| --- | --- | --- |
| #81 | **실제 WebGPU 선택 Adapter 식별** 및 software Vulkan ICD 고정 | 선택 adapter가 Vulkan+CPU임을 테스트에서 단언; adapter 메타데이터 artifact 보존; 실제 WGSL smoke 성공 |
| #82 | Geometry/Navigation 세부 단계의 측정(검증/준비/할당/전송/dispatch/readback/CPU 대체) | 측정 단계/수치 스키마·단위 테스트, 결정적 결과 및 타이밍 출력 보존(속도 gate 없음) |
| #83 | Scene/Geometry descriptor GPU buffer 재사용 및 변경 감지 | 동일 Scene 재할당·재업로드 없음, 변경 시 갱신·결과 동일, backend 간 혼동 방지 |
| #84 | Ray/Result buffer capacity 기반 재사용 | 배치 크기 증가·감소, 데이터 오염 없음, 생성/확장 카운터 및 결과 검증 |
| #85 | 리소스 수명/부분 실패 복구 | 반복 생성/폐기, 유효하지 않은 입력·한도 초과 후 정상 실행, 실패 시 CPU fallback 및 유효 상태 확인 |

### Phase B — 정확도 및 안전한 검증 경로

| 예정 PR | 구현 목표 | GitHub Actions 검증 조건 |
| --- | --- | --- |
| #86 | 중복 f64 preflight 경량화 및 검증용/벤치마크용 경로 분리 | authoritative 경로는 반드시 f64 검증·fallback; 실험용 raw 결과는 Geant4에 적용 금지 |
| #87 | FP32/FP64 경계면·접선·가까운 후보·동률 사례 | raw GPU 거리·boundary kind·daughter ID 오차 및 기대 fallback을 직접 단언 |
| #88 | Ray 수/Box 수/Scene 변경 조합 테스트 | raw WGSL 결과, Scene 교체, unsupported 조건의 CPU 대체 및 제한 검사 |
| #89 | Geant4 Navigator/Shadow 검증 범위 확대 | Geant4 기준과 거리/ID·state 비교 및 `geant4-webgpu-shadow` CI에서 실제 실행 확인 |

### Phase C — CI 성능 진단과 Physics 통합

| 예정 PR | 구현 목표 | GitHub Actions 검증 조건 |
| --- | --- | --- |
| #90 | 1–8192 rays, Daughter 개수별 결정적 bench matrix | CPU oracle/원시 GPU 결과, checksum/dispatch/대체 횟수 및 CSV·환경 artifact |
| #91 | 변경 전·후 timing 비교·분석 도구 | fixture 기반 중앙값/범위/시도 수 단위 테스트, 회귀 시간은 보고만 |
| #92 | Gamma geometry/process-competition/Compton Shadow 연결 검증 | 경계 선후관계, 에너지·방향·2차 입자·fallback, 독립 Geant4 검증 |
| #93 | Physics/RNG 통계 및 Geant4 상태 회귀 | 사전 정의된 통계 오차, seed/counter 재현성, energy/momentum 보존 |

### 범위와 완료 조건

1. PR을 생성하기 전에 이미 구현된 기능이면 **중복 PR을 만들지 않는다**.
2. #81 → #82의 관측 결과를 바탕으로 #83–#85의 재사용 범위를 결정한다.
3. CPU f64 oracle을 제거하거나 GPU 결과를 검증 없이 authoritative transport에
   적용하는 최적화는 허용하지 않는다. Raw GPU 실행은 실험 API에 격리한다.
4. Shared CI의 software Vulkan은 실제 GPU 메모리 대역폭, 하드웨어 동시성,
   벤더별 호환성·가속 성능을 입증하지 못한다.
5. 실물 GPU가 확보되면 별도 [검증 프로토콜](physical-gpu-validation-protocol.md)로
   성능을 측정하고, 이 로드맵의 완료와 구분해 보고한다.
