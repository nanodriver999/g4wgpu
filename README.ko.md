# g4wgpu

Geant4의 기존 C++ 애플리케이션 구조를 유지하면서 계산 집약적인 physics/geometry kernel을
WebGPU compute로 단계적으로 가속하는 연구 프로젝트입니다.

기본 native WebGPU runtime은 **wgpu-native**를 목표로 하며,
상위 C++ 계층은 가능한 한 표준 **webgpu.h** API에 맞춰 runtime 종속성을 격리합니다.

## 현재 상태

현재 단계는 Geant4 11.5.0.beta 소스 구조 분석과 구현 계획 수립입니다.

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

## 라이선스

이 저장소의 자체 작성 코드는 MIT 라이선스를 기본으로 합니다.

Geant4는 별도의 Geant4 Software License를 사용합니다.
Geant4 소스에서 직접 파생되는 코드가 추가될 경우 해당 라이선스 의무를 파일 단위로 검토합니다.
