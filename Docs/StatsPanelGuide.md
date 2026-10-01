# StatsPanel로 병목 찾기

StatsPanel은 **어느 구간부터 개선해야 하는지** 찾는 도구다. 먼저 위쪽의 병목 후보를 보고, 해당 구간의 상세 수치만 펼쳐서 확인한다.

현재 구현 설명은 `32d03c4` 기준이다. 최적화 적용 상태와 남은 개선점은 [엔진 최적화 검토](EngineOptimizationReview.md)를 참고한다.

StatsPanel을 X 버튼이나 Window 메뉴로 닫으면 상세 Stat 수집과 GPU 타임스탬프·마커 기록을 중단한다. 다시 열면 기존 체크 상태대로 수집을 재개하며, 이전 표본은 유지한다. Frame / Picking은 패널을 닫아도 개별 활성 설정에 따라 계속 수집한다. 창을 접거나 다른 탭에 가리는 것은 닫기와 다르다.

중단되는 것은 Stats용 측정과 기록이다. 컬링 알고리즘의 내부 결과·예산 판단, 기본 렌더링과 내부 카운터 계산까지 제거하는 기능은 아니다.

## 1. 처음에는 이렇게 본다

1. 비교할 씬과 카메라를 고정하고, 가능하면 뷰포트를 하나만 사용한다.
2. GPU도 확인하려면 `Enable GPU breakdown`을 누른다. 이 버튼은 GPU 시간 측정을 켜며, GPU 컬링 모드를 바꾸지는 않는다.
3. 씬 로딩이 끝나면 `Reset samples`를 누른다. 이전 씬의 평균과 최댓값이 섞이지 않게 하는 과정이다.
4. `CPU scene`, `Opaque CPU`, `GPU passes`에 표시된 후보를 확인한다.
5. 후보 아래의 `Next`를 따라 해당 항목을 확인한다. 설정은 한 번에 하나씩 바꾸고, 바꿀 때마다 초기화한다.

예를 들어 CPU scene에서 Opaque Submit이 가장 크고, 그 안에서는 Material collect + update가 가장 크다면 **재질 조회와 갱신부터 확인한다.** 이 상황에서 BVH부터 수정할 이유는 없다.

## 2. 패널 위쪽의 표시

| 표시 | 의미 |
| --- | --- |
| TOP CANDIDATES (1s avg) | 최근 1초 평균이 가장 큰 구간과 비용이 비슷한 공동 후보다. |
| Next | 그 구간이 크면 먼저 확인할 대상이다. |
| Measured stages: 3 / 6 | 최근 표본이 있는 구간이 6개 중 3개라는 뜻이다. 꺼진 항목이나 실행되지 않은 경로는 순위에서 빠진다. |
| Stage details | 각 구간의 시간과 비교 막대를 펼친다. |
| No recent sample | 최근 표본이 없다. 비용이 0이라는 뜻은 아니다. |
| Off | 해당 항목의 측정이 꺼져 있다. |

막대 길이는 **그룹에서 가장 큰 값을 기준으로 한 상대 크기**다. 프레임에서 차지하는 비율이 아니다.

후보·막대·순위는 최근 1초 동안 패널이 받아들인 새 Event 표본의 평균을 사용하고, 0.5초마다 갱신한다. 두 항목의 차이가 0.05ms 이하이거나 큰 값의 10% 이하이면 기존 순서를 유지한다. 최댓값과 이 범위 안에 있는 항목은 공동 후보로 표시한다. 그래서 아주 조금 더 큰 값이 아래에 남아 있을 수 있다.

여러 뷰의 호출이나 한꺼번에 도착한 GPU 결과도 표본 횟수로 가중해 평균을 계산한다. 프레임 합계가 아니라 호출당 평균이며, GPU의 1초 구간은 결과 수신 시점 기준이다. 패널을 닫거나 Reset samples를 누르면 순위용 기록을 비우고, 새 표본부터 다시 계산한다. 패널이 1초 넘게 표시되지 않은 경우에도 순위용 기록을 다시 시작한다. CPU와 GPU 후보는 따로 읽는다.

상세 표의 Last / Avg / Max는 기존 의미를 유지한다. 순간 지연은 Last와 Max로, 초기화 이후 전체 평균은 Avg로 확인한다.

## 3. CPU에서 큰 구간에 따라 확인할 것

| 큰 구간 | 먼저 확인할 것 | 개선 방향 |
| --- | --- | --- |
| Capture World CPU | 전체 오브젝트 수, 매 프레임 반복하는 장면 정보 수집 | 변하지 않는 정보의 재수집과 임시 데이터 생성을 줄인다. |
| Cull | 컬링 모드, Visible, GPU 모드의 Result Map Wait | 컬링 자체가 비싼지, GPU 결과를 기다리는 비용인지 구분한다. |
| Packet Build CPU | Packets (Active View), 가시 오브젝트 수 | 패킷과 행렬을 매번 만드는 비용을 줄인다. |
| Opaque Submit CPU | 아래 Opaque CPU의 후보 | 정렬·재질·업로드·드로우·워커 중 큰 구간으로 내려간다. |

### Opaque CPU 세부 구간

| 큰 구간 | 의미와 개선 방향 |
| --- | --- |
| Sort CPU | 현재 `RenderOpaque()`의 정렬과 측정 코드가 주석 처리돼 있어 새 표본이 생기지 않는다. 정렬 비용이 0으로 최적화됐다는 뜻은 아니다. |
| Material collect + update | 재질 조회와 파라미터 갱신 비용이다. 같은 재질을 반복 조회하거나 동일한 값을 다시 올리는지 확인한다. |
| Immediate bulk CB upload | 즉시 컨텍스트에서 버퍼를 준비하고 Map·복사·Unmap하는 비용이다. CB Written Bytes와 함께 보고 업로드 횟수와 양을 줄인다. |
| Immediate bind + draw | 상태 바인딩과 Draw 호출 비용이다. Draw Calls가 많으면 먼저 호출 수와 중복 바인딩을 줄인다. 개별 CB 업로드로 우회하는 경로의 비용도 포함된다. |
| Worker record + completion wait | 병렬 명령 기록을 시작해서 완료할 때까지 걸린 시간이다. 워커별 작업량 편차와 명령 기록 비용을 확인한다. |
| Execute command lists | 기록한 명령 목록을 CPU에서 제출하는 비용이다. 명령 목록 수와 드라이버 호출 부담을 확인한다. |

**Worker 시간이 크다고 해서 대기만 오래 걸린다고 판단하면 안 된다.** 실제 워커 작업과 스케줄링·완료 대기가 함께 들어 있다. 현재 패널에는 워커별 시간이 없으므로, 여기서 원인이 더 좁혀지지 않으면 워커별 기록 시간을 추가하는 것이 다음 순서다. 워커 수부터 늘리지는 않는다.

Immediate와 Worker 경로는 실행 방식에 따라 일부만 측정되는 것이 정상이다. 실행되지 않는 경로의 No recent sample을 오류로 볼 필요는 없다.

현재 `RenderOpaque()`는 입력 패킷을 불투명·반투명으로 분리하지 않고 처리하며 `RenderTranslucent()`는 비어 있다. 따라서 Opaque라는 표시 이름만으로 불투명 메시만의 비용이라고 단정하지 않는다.

## 4. GPU에서 큰 구간에 따라 확인할 것

| 큰 구간 | 먼저 해볼 비교 | 결과를 읽는 방법 |
| --- | --- | --- |
| Opaque / Pass | 같은 카메라에서 해상도를 낮춘다. 그다음 LOD를 비교한다. | 해상도에 민감하면 픽셀 처리 쪽을, LOD에 민감하면 지오메트리 처리 쪽을 우선 확인한다. 이 비교만으로 특정 셰이더가 원인이라고 확정하지는 않는다. |
| HZB Build / Pass | GPU 컬링을 켠 경우와 끈 경우를 비교한다. | HZB 생성에 쓴 비용만큼 전체 렌더링 비용이 줄어드는지 본다. |
| Occlusion Dispatch / Pass | 검사 대상 수와 컬링 모드를 비교한다. | GPU 가시성 검사 비용이다. CPU가 결과를 기다리는 시간은 별도다. |
| Grid / Pass | 그리드 표시를 켜고 끈다. | 그리드가 렌더링 비용에 얼마나 기여하는지 확인한다. |
| Editor overlays / Pass | 바운드·아웃라인·기즈모 등의 표시를 하나씩 끈다. | 비용이 줄어드는 표시 기능부터 확인한다. |

`Viewport Render`는 GPU 렌더링 전체 구간이고 `Opaque / Pass`는 그 안의 메시 렌더링 구간이다. 둘이 비슷하면 측정된 GPU 시간 대부분을 메시 렌더링에 쓰고 있다는 뜻이다. **둘을 더하지 않는다.** Viewport Render에는 UI와 Present가 포함되지 않는다.

현재 LOD 적용부에는 섹션별 인덱스 범위를 전체 메시 범위로 덮어쓰는 문제가 있다. CPU 소프트웨어 컬링 경로에서는 자동 LOD 선택을 건너뛰는 경우도 있다. LOD 비교 전 실제 선택 단계와 섹션 범위가 맞는지 확인한다. 상세 내용은 [메시 최적화 검토](EngineOptimizationReview.md#3-메시-최적화)를 참고한다.

GPU 결과는 나중에 도착한다. 현재 화면의 CPU Last와 GPU Last가 정확히 같은 프레임의 값이라고 해석하지 않는다.

## 5. GPU 컬링을 켰는데 오히려 느리다면

먼저 상세 표의 `GPU Culling CPU → Result Map Wait`를 확인한다.

- **Result Map Wait가 크다:** GPU 결과를 CPU에서 읽는 Map 호출이 오래 걸린다. 동기화와 결과 회수 방식을 먼저 확인한다. 이 값은 Map 호출 전체 시간이며 순수 대기 시간만 분리한 값은 아니다.
- **HZB 또는 Dispatch가 크다:** GPU 컬링 자체에 드는 비용을 확인한다.
- **Visible이 거의 줄지 않는다:** 컬링으로 절약하는 그리기 작업이 적다. 이 씬에서는 컬링을 켜는 것이 손해일 수 있다.

컬링의 효과는 제거 개수만으로 판단하지 않는다. 같은 장면에서 **최종 Frame Time이 줄었는지** 확인한다.

GPU 컬링 모드에서는 현재 구현상 Frustum Rejected에 오클루전 제거분도 포함된다. CPU 전용 수치가 0으로 보일 수도 있다. 이 경우 CPU 컬링과 동일한 분류라고 가정하지 않는다.

## 6. 상세 표 읽기

| 열 | 읽는 방법 |
| --- | --- |
| Current / Last | Gauge는 저장된 현재값, Event는 마지막 1회 측정값, FrameSum은 해당 프레임의 합계다. |
| Avg | 초기화 이후 모든 Event 표본의 평균이다. 지속적인 비용을 비교할 때 본다. |
| Max | 초기화 이후 가장 큰 Event 표본이다. 간헐적인 끊김을 찾을 때 본다. |
| Count | Event 측정 횟수다. 반드시 프레임 수와 같지는 않다. |

체크박스는 항목의 수집을 켜고 끈다. 다시 켰을 때 이전 평균이 필요 없다면 Reset samples를 누른다. Gauge와 FrameSum은 Avg·Max·Count 대신 현재값 위주로 본다.

**Avg도 크면 반복 비용이고, Avg는 작지만 Max만 크면 순간 지연부터 조사한다.** Max에는 로딩이나 초기화 비용도 남을 수 있으므로, 씬이 안정된 후 초기화한 결과로 비교한다.

### Picking: 현재 측정 범위

현재 에디터는 유효한 뷰에서 좌클릭하고 기즈모를 조작하거나 가리키지 않을 때 피킹한다. 매 프레임 수행하는 작업이 아니다. `PickActiveView()`는 후보 전체 수집·정렬 대신 `World.TraceLineClosest()`로 객체 BVH를 가까운 노드부터 순회하고, 후보를 만날 때 정밀 검사를 호출한다. 실제 Hit로 갱신한 최근접 거리보다 먼 노드는 건너뛴다.

| 항목 | 현재 의미 |
| --- | --- |
| Total | 레이 컨텍스트 준비, BVH 탐색·정밀 검사, 관련 통계 기록을 포함한 피킹 처리 구간이다. 앞선 마우스 좌표 역투영과 이후 Outliner 선택 적용은 포함하지 않는다. |
| Narrow | 탐색 중 호출한 `World.LineTraceCandidate()`의 누적 시간이다. 가시성 확인, 컴포넌트별 검사, 로컬 레이 변환과 메시 교차 검사 등이 포함되며 삼각형 검사만의 시간은 아니다. |
| Broad | `World.TraceLineClosest()` 전체 경과 시간에서 Narrow 누적 시간을 뺀 값이다. 객체 BVH의 AABB 검사·순회, leaf 내부 정렬, 콜백 호출 주변 비용 등이 포함된다. |
| Candidates / Pick | 정밀 검사 콜백 호출 횟수다. 전체 AABB 통과 후보 수, BVH 노드 수, 삼각형 검사 수가 아니다. Bounds를 우회하는 Billboard 등의 호출과 가시성 검사에서 바로 탈락한 호출도 포함한다. |

Candidates / Pick이 작으면 최근접 거리 가지치기로 검사 호출이 줄었을 수 있다. 씬의 전체 객체 수나 검사한 삼각형 수가 적다는 뜻은 아니다.

Broad와 Narrow는 순차적인 두 패스가 아니라 탐색 중 번갈아 실행되는 작업이다. Broad 또는 Narrow 중 하나만 켜도 Narrow 내부 시간을 측정해야 Broad를 분리할 수 있다. Total에는 탐색 바깥의 준비·기록도 있어 `Total = Broad + Narrow`가 정확히 성립하지 않는다.

### 과제의 피킹 측정 기준과 비교

발제 예시는 마우스 좌표로 Pick Ray를 만든 **이후** 카운터를 시작해 전체 객체 탐색·교차 판정을 측정한다. BVH를 사용해도 보고 대상은 그 전체 피킹 처리 시간이다. **대회 Picking ms에는 Total을 사용하고 Narrow만 따로 보고하지 않는다.** 현재 Total은 레이 생성 이후의 처리 범위를 포함하며, 컨텍스트 준비와 일부 통계 기록도 포함하므로 순수 교차 연산 시간이라고 표기하지 않는다.

뷰포트의 Last Pick은 Total의 최근값, Num Attempts는 Total 표본 수, Accumulated Time은 Total 누적값이다. 대회 측정 중에는 Total을 활성화하고 같은 표본 집합의 값으로 비교한다. Candidates / Pick은 한 번의 피킹에서 호출한 후보 검사 수이지 전체 피킹 시도 횟수가 아니다. 통계나 예산용 수치를 줄이기 위해 실제 탐색 비용을 측정 구간 밖으로 옮기지 않는다.

코치 안내에서 금지한 캐시는 이전 화면을 재사용하며 Draw를 생략하는 방식이다. 변경 없는 월드 행렬·메시 데이터의 캐시는 가능하지만 매 프레임 객체별 Draw를 유지하고, 부모 이동에 따른 자식 Bounds·BVH 갱신을 반영해야 한다. 테스트에서 메시가 움직이지 않더라도 이 갱신 기능을 생략한 결과를 일반적인 최적화 성능으로 제시하지 않는다. 마우스 위치에 맞는 메시 선택이 틀리면 0점이므로, 시간 비교보다 선택 정확성 확인이 먼저다.

Broad가 크면 객체 BVH 순회와 leaf 후보 수를, Narrow가 크면 호출 수와 메시 내부 검사를 확인한다. 현재 개선 대상으로는 불필요한 컴포넌트 Bounds 조회 정리가 남아 있다. Bounding Sphere 적용은 보류한다.

측정 근거: `MultipleViewportsAdapter.cpp`의 `TracePickCandidate()`·`PickActiveView()`, `World.cpp`의 `LineTraceCandidate()`, `PrimitiveBVH.cpp`의 `TraceRayClosest()`·`TraverseRayClosest()`.

## 7. 잘못 판단하기 쉬운 부분

- CPU scene 후보는 등록된 일부 구간 중 최댓값이다. 입력·UI·Present 등 엔진 전체의 병목을 확정하는 결과는 아니다.
- Cull과 Packets는 활성 뷰 기준이다. CPU/GPU 패스 시간은 호출별이고 Last는 마지막 호출값이다. Draw Calls·Triangles·CB Written Bytes는 렌더링한 뷰 전체의 프레임 합계다. 여러 뷰가 켜져 있으면 범위가 달라지므로 우선 단일 뷰에서 비교한다.
- 현재 `RenderMultipleViewports()`는 0번 뷰만 제출한다. 이름만 보고 네 뷰를 모두 렌더링한 비용으로 해석하지 않는다. 향후 다중 뷰 제출을 복구하면 위 집계 범위를 다시 확인한다.
- Opaque Submit에는 Opaque CPU 세부 구간들이 포함된다. 부모 시간과 자식 시간을 합산하면 중복 계산이다.
- GPU 측정을 켜지 않았거나 최근 결과가 없다면 GPU 비용을 판단할 수 없다. Profiler Frames Skipped가 반복되면 일부 프레임의 측정이 생략되고 있다는 뜻이다.
- CPU와 GPU는 겹쳐서 작업하므로 두 시간을 더한 값이 Frame Time은 아니다.

개선 순서는 **후보 확인 → 관련 수치 확인 → 한 가지 변경 → Frame Time과 해당 구간의 Avg 비교**다. 해당 구간만 빨라지고 Frame Time은 그대로라면 전체 성능을 제한하는 다른 구간이 남아 있는 것이다.

## 8. PIX와 함께 쓰는 순서

**StatsPanel에서 조사할 구간을 고르고, PIX에서 그 구간이 비싼 원인을 찾는다.** 수정 후에는 원래 D3D11 실행으로 돌아와 StatsPanel에서 개선 효과를 확인한다.

현재 엔진에는 GPU 타임스탬프와 `ID3DUserDefinedAnnotation::BeginEvent/EndEvent`가 이미 있다. GPU 캡처를 시작하기 위해 새 프로파일러를 만들 필요는 없다. D3D11On12는 이 annotation을 PIX 마커로 변환한다. [Microsoft: D3D11 앱을 PIX로 분석하기](https://devblogs.microsoft.com/pix/debugging-d3d11-apps-using-d3d11on12/)

### 준비: 실행 파일과 PDB

- 현재 프로젝트에는 별도 Profile 구성이 없으며, `Release`에 최적화와 심볼 생성이 모두 설정돼 있다.
- Premake 기준 실행 파일은 `Build/Bin/Release-windows-x64/HitoriEditor.exe`다. 실제 사용 중인 빌드 출력 위치를 확인해서 선택한다.
- PIX의 Working Directory는 저장소 루트로 지정한다. 이 엔진은 에셋과 셰이더를 상대 경로로 읽는다.
- 실행 파일과 **동일한 빌드에서 나온 PDB**를 준비하고 PIX의 심볼 검색 경로에 그 폴더를 추가한다. 함수 이름이 주소로만 나오면 심볼 설정과 PDB 일치 여부를 먼저 확인한다. [Microsoft: PDB 설정](https://learn.microsoft.com/en-us/windows/win32/direct3dtools/pix/articles/timing-captures/pix-timing-captures-pdb-config)

### CPU 후보가 크면: Timing Capture

1. PIX에서 에디터를 실행한다. 이 단계는 원래 D3D11 실행을 기준으로 한다.
2. 문제의 씬을 열고 로딩이 끝난 뒤 StatsPanel에서 후보를 확인한다.
3. Timing Capture 옵션에서 `CPU Samples`를 켠다. 기다리는 구간도 조사하려면 `Context Switches Callstacks`를 함께 켠다.
4. 문제가 반복되는 동안 짧게 캡처하고, Timeline에서 느린 시간대를 선택한다.
5. CPU 샘플의 호출 트리에서 후보 함수와 그 하위 함수를 찾아, 어느 작업에 샘플이 집중되는지 확인한다.

CPU 샘플링은 추가 엔진 계측 없이 실행 중인 함수를 조사하는 방법이다. **모든 함수 호출의 정확한 시작·끝 시간을 기록하는 기능은 아니다.** 지금의 FStatScope도 PIX CPU 이벤트를 생성하지 않으므로 StatsPanel의 항목이 그대로 타임라인 막대가 되지는 않는다. [Microsoft: Timing Capture](https://learn.microsoft.com/en-us/windows/win32/direct3dtools/pix/articles/timing-captures/pix-timing-captures)

| StatsPanel의 후보 | PIX에서 볼 대상 | 판단 방향 |
| --- | --- | --- |
| Capture World CPU | `FMultipleViewportsAdapter::CaptureWorld` 아래의 순회·할당·복사 | 반복 수집 중 어떤 작업이 CPU를 많이 쓰는지 찾는다. |
| Packet Build CPU | `BuildRenderPackets` 아래의 패킷 생성·행렬 계산과 관련 워커 스레드 | 함수 전체에는 컬링도 있으므로 패널의 Packet Build 시간과 동일 범위로 보지 않는다. |
| Sort CPU | 현재 정렬 호출·측정이 비활성화돼 있음 | 정렬을 복구한 뒤에만 정렬 호출과 비교 함수의 비용을 조사한다. |
| Picking Broad / Narrow | `TraceLineClosest` → `TraverseRayClosest` → `TracePickCandidate` → `LineTraceCandidate` | 순회와 정밀 검사가 섞여 실행된다. Broad는 전체 순회 시간에서 Narrow를 뺀 값이며, 후보 전체 수집·정렬 경로를 찾으면 현재 에디터 경로와 다르다. |
| Worker record + completion wait | 메인 스레드와 워커 스레드들의 같은 시간대 | 일부 워커만 오래 일하는지, 모두 바쁜지, 메인이 기다리는지 확인한다. |
| Result Map Wait | 결과 회수의 `Map` 주변 호출 스택과 스레드 상태 | CPU 연산보다 동기화가 문제인지 조사한다. |

CPU 샘플이 적다고 비용이 작은 것은 아니다. 잠든 스레드의 대기는 실행 샘플에 충분히 드러나지 않으므로 스레드 상태와 context switch를 함께 본다. 반대로 파이버 전환이나 바쁜 대기는 일반적인 OS 스레드 대기와 다르게 보일 수 있다. [Microsoft: 대기와 Context Switch 분석](https://devblogs.microsoft.com/pix/analyzing-stalls-and-context-switches-in-timing-captures/)

### GPU 후보가 크면: GPU Capture

1. PIX의 GPU 캡처용 실행 설정에서 **Force D3D11On12**를 켜고 에디터를 새로 실행한다.
2. 같은 씬과 카메라로 맞춘다.
3. StatsPanel에서 **Enable GPU breakdown**을 누르고 패널을 열어 둔다. 패널을 닫거나 해당 Stat을 끄면 annotation도 생략한다.
4. 화면이 안정된 뒤 GPU Capture로 한 프레임을 잡는다.
5. Events에서 아래 마커를 찾아 펼친다. 필요하면 캡처의 Timing Data를 수집해 구간을 비교하고, 의심되는 Draw나 Dispatch의 Pipeline/State를 확인한다. [Microsoft: GPU Capture](https://devblogs.microsoft.com/pix/gpu-captures/)

| StatsPanel 이름 | 현재 코드의 PIX 마커 이름 |
| --- | --- |
| Viewport Render | `Viewport Render` |
| Opaque / Pass | `Opaque Immediate` 또는 `Opaque Command Lists` |
| HZB Build / Pass | `HZB Build` |
| Occlusion Dispatch / Pass | `Occlusion Dispatch` |
| Grid / Pass | `Grid` |
| Editor overlays / Pass | `Editor Overlays` |

변환된 이름 앞에 `D3D11 Event:`가 붙을 수 있다. 실행한 경로의 마커만 나타나며, 여러 뷰가 켜져 있으면 같은 이름이 반복될 수 있다. D3D11On12가 생성한 추가 이벤트와 API 호출도 보인다. [Microsoft: annotation 변환](https://devblogs.microsoft.com/pix/debugging-d3d11-apps-using-d3d11on12/)

예를 들어 Opaque가 가장 크다면 해당 마커 안을 펼쳐 **많은 Draw가 조금씩 비싼지, 특정 Draw가 유독 비싼지**부터 구분한다. 특정 Draw가 비싸면 해당 메시·인덱스 수·바인딩된 리소스·렌더 상태를 확인하고, 해상도와 LOD를 하나씩 바꿔 비교한다.

### 두 도구의 시간이 다를 때

StatsPanel의 평상시 D3D11 시간과 PIX의 D3D11On12 GPU Capture 시간을 직접 비교해 개선율을 계산하지 않는다. 후자는 변환과 캡처 재생을 거친 실행이다. **PIX에서 찾은 원인을 수정한 뒤, 원래 실행 환경의 같은 씬에서 Frame Time과 해당 구간의 Avg가 줄었는지 확인한다.**

D3D11의 Timing Capture에는 GPU 작업 표시 제약이 있다. CPU Timing Capture에서 GPU 구간이 보이지 않는다고 엔진이 GPU를 쓰지 않는다고 해석하지 않는다. GPU 작업 조사는 위의 별도 GPU Capture 절차로 진행한다. 또한 D3D11On12에서는 원본 HLSL 수준 디버깅과 셰이더 리플렉션 등에 제약이 있다. [Microsoft: D3D11On12 제약](https://devblogs.microsoft.com/pix/debugging-d3d11-apps-using-d3d11on12/)

### 추가 마커는 언제 필요한가

CPU 샘플로도 작업의 시작·끝이나 워커 간 관계가 불명확할 때만 `WinPixEventRuntime`을 추가하고, 해당 큰 작업에 CPU 이벤트를 넣는다. 예를 들어 `CaptureWorld`, 패킷 생성, 워커 청크의 명령 기록처럼 실제로 구분해야 하는 범위에 추가한다. 오브젝트나 삼각형마다 이벤트를 찍지는 않는다.

현재 GPU 마커를 중복해서 추가할 필요는 없다. CPU 의미 구간을 표시하는 이벤트와 기존 GPU annotation은 용도가 다르다.

이 절차는 현재 소스와 Microsoft 문서를 확인해 작성했다. 이 저장소의 실행 파일로 PIX 캡처에 성공했는지는 아직 검증하지 않았다.
