# Ath12kWifi

Start with the [adaptation procedure](/porting/network) for board inputs, source changes, build integration and verification order.

Ath12kWifi implements an ath12k-based Wi-Fi stack with PCI, MHI, QRTR, and
board-supplied interfaces. The source includes QCC2072, QCN9274, and WCN7850
hardware definitions.

| Field | Value |
| --- | --- |
| Repository | [UniXNU/Ath12kWifi](https://github.com/UniXNU/Ath12kWifi) |
| Function | ath12k Wi-Fi transport and driver integration |
| Board inputs | PCI access, interrupts, firmware and board callbacks |
| Hardware verification | Record device-specific results in the device page |

Read the component README and `nw_board` interfaces before selecting it. The
catalog records its source revision, OS requirements, build command, and outputs.
Use the component's host checks and the board's network verification procedure.
