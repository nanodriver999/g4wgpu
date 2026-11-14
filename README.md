# g4wgpu

Geant4의 기존 C++ 애플리케이션 구조를 유지하면서 계산 집약적인 physics/geometry kernel을
WebGPU compute로 단계적으로 가속하는 연구 프로젝트입니다.

기본 native WebGPU runtime은 **wgpu-native**를 목표로 하며,
상위 C++ 계층은 가능한 한 표준 **webgpu.h** API에 맞춰 runtime 종속성을 격리합니다.

## 현재 상태

초기 분석 및 구현 계획 이후 CPU reference, WebGPU backend, Geant4 tracking-manager 연동과 검증용 geometry/physics 경로를 단계적으로 구현했습니다. 아래 기능은 연구용 프로토타입이며 전체 Geant4 tracking을 GPU로 대체하지 않습니다.

현재 검증 범위:

- CPU reference 및 WebGPU Klein–Nishina batch sampler, Geant4 gamma shadow integration (`docs/geant4-webgpu-shadow.md`)
- Geant4 `G4VTrackingManager` 확장과 선택적 Compton 경로의 회귀 검증; 지원되지 않는 track/process는 CPU 경로 유지
- Box 기반 flattened geometry의 opt-in fused WebGPU next-boundary 실험 (`docs/flat-box-fused-webgpu.md`); Geant4 CPU navigator가 authoritative 경로로 유지됨
- 배치별 end-to-end wall-time CSV 벤치마크 및 환경 메타데이터 (`docs/flat-box-fused-benchmark.md`)

GitHub Actions는 CPU, WebGPU(software Vulkan), Geant4, 결합 통합 경로를 검사합니다. 소프트웨어 Vulkan 통과는 실제 GPU 하드웨어의 속도 향상이나 모든 geometry/physics 상황에서의 정확도를 입증하지 않습니다. `docs/implementation-plan.md`는 초기 단계별 연구 계획이며, 실제 구현 범위와 제약은 각 기능 문서를 참조하세요.

기준 Geant4:
`nanodriver999/geant4@f3d5293d384757b8a228a099898b2b87cfa4023c`

핵심 전략:

```text
Geant4 C++
   ↓
G4VTrackingManager
   ↓
batched Track SoA
   ↓
ComputeBackend
   ├─ CPU reference
   └─ WebGPU
        ↓
     wgpu-native
        ↓
 Vulkan / Metal / D3D12
```

전체 Geant4를 재작성하지 않습니다. GPU에 적합한 kernel만 선택적으로 이동하고 CPU fallback을 유지합니다.

## 문서

- [Geant4 GPU 전환 분석](docs/geant4-gpu-analysis.md)
- [단계별 구현 계획](docs/implementation-plan.md)

## 개발 원칙

- 기존 C++ Geant4 application을 유지합니다.
- Geant4 core를 직접 fork-modify하는 것보다 public extension point를 우선합니다.
- `G4VTrackingManager::HandOverOneTrack/FlushEvent`를 batching 경계로 활용합니다.
- GPU buffer에는 `G4Track` object 자체가 아니라 POD/SoA 데이터만 전달합니다.
- CPU reference backend를 항상 유지합니다.
- 정확도와 Monte Carlo 통계 검증을 성능보다 먼저 통과시킵니다.
- WebGPU portable path와 native-only extension을 구분합니다.

## License

이 저장소의 자체 작성 코드는 MIT 라이선스를 기본으로 합니다.

Geant4는 별도의 Geant4 Software License를 사용합니다.
Geant4 소스에서 직접 파생되는 코드가 추가될 경우 해당 라이선스 의무를 파일 단위로 검토합니다.
