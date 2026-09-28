# Project_ComStack_AUTOSAR
Mục tiêu của dự án là nghiên cứu và phát triển ECU dựa trên kiến trúc Layered - AUTOSAR Classic Platform với module COM. Mô tả Data Path qua từng Layer từ đó phát triển các dịch vụ Diagnostic gửi các Request chẩn đoán đến eVCU của Khách hàng.
Dự án phát triển ECU dựa trên kiến trúc Microcontroller STM32F103C8T6 Bulepill
> Đây là dự án nghiên cứu và học tập, phát triển Porfolio cá nhân. Dự án sẽ không tuân thủ đầy đủ các Standard trong kiến trúc AUTOSAR Classic Platform cũng như ISO 26262,...


## Mục lục
1. [Mô hình dự án](#1-mô-hình-dự-án)
2. [Kiến trúc hệ thống](#2-kiến-trúc-hệ-thống)
3. [Công cụ & Môi trường phát triển](#3-công-cụ--môi-trường-phát-triển)
4. [Kiểm thử và xác minh](#4-kiểm-thử-và-xác-minh)
5. [Kết quả kiểm thử](#5-kết-quả-debug)
---

## 1. Mô hình dự án

Việc phát triển ECU trong ngành công nghiệp Oto hiện đại sử dụng kiến trúc phần mềm phân lớp theo chuẩn AUTOSAR Classic Platform để tách biệt các tầng như Application, Communication Services, ECU Abstraction, MCAL, Driver.

Các mục tiêu chính:
* Hiểu và triển khai phần mềm theo kiến trúc phân lớp
* Nghiên cứu luồng truyền dữ liệu giữa các tầng của Communication Stack.
* Xây dựng Driver giao tiếp với phần cứng.
* Mô tả các Diagnostic Service để gửi và nhận phản hồi chẩn đoán ECU Khách hàng tương đối với thực tế.
* Thực hành debug và kiểm chứng hệ thống thông qua trình mô phỏng ECU Renode

Thành phần dự án:
- _DiagECU_: Mô tả cung cấp các dịch vụ chẩn đoán với ECU khách hàng bằng UDS thông qua CAN-TP
- _ComECU_: Mô tả các tín hiệu cập nhật định kỳ Engine status và nhận Veicle Command từ ECU khách hàng thông qua COM

```text
DiagECU/
├── .vscode/               # Cấu hình Workspace VS Code

├── BSW/
│   ├── Common/            # Định nghĩa các kiểu dữ liệu chuẩn dùng chung (Std_Types, ComStack_Types)
│   ├── EcuAbstraction/    # Trừu tượng ECU (Chứa CanIf, CanTp, LinIf)
│   ├── Mcal/              # Trừu tượng vi điều khiển (Chứa Driver CAN, LIN)
│   └── Services/          # Tầng dịch vụ hệ thống (Chứa các module COM, DCM, PduR)
│
├── Config/                # File cấu hình từng module: CanIf_Cfg, PduR_Cfg,...
│
├── Platform/              # Mô tả nền tảng phần cứng
│   ├── Bsp/
│   ├── Debug/
│   └── Spl/               # Thư viện SPL của STM32F103 (Standard Peripheral Library)
│
├── Script/                # Chứa script (.resc) định nghĩa môi trường và mạng CAN ảo cho Renode
│
├── build/                 # Các file biên dịch object (.o) và file thực thi (.elf, .bin, .hex)
│
├── main.c
├── Makefile               # Hỗ trợ biên dịch bằng GCC
├── stm32f103.ld           # Định nghĩa phân vùng nhớ (Flash/RAM) cho STM32F103
└── startup_stm32f103.s    # Chứa mã khởi động (Vector Table, Reset Handler)

```
---

## 2. Kiến trúc hệ thống

* **Application (SWC):** Chứa các Software Components thực thi logic (Diagnostic, Engine Status).
* **Services Layer:**
    * **COM (Communication):** Đóng gói/giải nén và điều phối việc truyền/nhận các tín hiệu mạng định kỳ (Signals/I-PDUs).
    * **DCM (Diagnostic Communication Manager):** Tiếp nhận, phân luồng dịch vụ chẩn đoán UDS và Parse các gói tin phản hồi.
    * **PduR (PDU Router):** Định tuyến luồng dữ liệu (PDU) giữa các module giao tiếp như COM, DCM và các giao thức phần cứng.
* **ECU Abstraction & MCAL:**
    * **CanIf, CanTp:** Phân mảnh/hợp nhất các khung giao tiếp CAN độ dài lớn (ISO-TP).
    * **MCAL (CAN, GPIO, v.v.):** Các Driver điều khiển trực tiếp thanh ghi vi điều khiển STM32F103.

## 3. Công cụ & Môi trường phát triển
1. **Vi điều khiển & Thư viện**: Sử dụng vi điều khiển STM32F103. Sử dụng thư viện SPL (Standard Peripheral Library)

3. **Trình biên dịch (Compiler):** Sử dụng bộ biên dịch GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`).

5. **Hệ thống Build:** Quản lý quá trình biên dịch bằng `Makefile`. Định nghĩa cấu trúc, tự động việc tạo file thực thi (`.elf`).

7. **Mô phỏng phần cứng:** Ứng dụng Renode để giả lập môi trường thực thi đa vi điều khiển. Thiết lập mạng CAN ảo (CAN Hub) kết nối đồng thời 3 Node độc lập (eVCU, DiagECU, COMECU).

9. **Gỡ lỗi (Debugger)**: Sử dụng GDB (`arm-none-eabi-gdb`) thông qua extension Cortex-Debug. GDB kết nối trực tiếp vào các cổng mạng (port 3333, 3334, 3335).

11. **Môi trường (IDE)**: Visual Studio Code.


## 4. Kiểm thử và xác minh

Hệ thống được kiểm tra thông qua nhiều phương pháp:

* Debug bằng GDB.
* Theo dõi luồng thực thi bằng Breakpoint.
* Kết hợp UART để in log phục vụ quá trình kiểm thử

## 5. Kết quả debug
### 5.1: Đối với các dịch vụ Diagnositc: Data được trace bằng breakpoint đến Driver thực tế sẽ được nạp vào mailbox của STM32F103
### a. Service: Session Control - 0x1003:
- Request:
<img width="1410" height="1410" alt="image" src="https://github.com/user-attachments/assets/6e5a6c66-8e85-4b6b-b0e5-91b237a62bf0" />
<img width="2564" height="1406" alt="image" src="https://github.com/user-attachments/assets/5e3c49d7-2069-4821-96d3-9006a21f37a9" />

- Response:
<img width="2564" height="1411" alt="image" src="https://github.com/user-attachments/assets/afc81073-1af3-463f-83fc-055dcea889c0" />


### b1. Service: ReadDataByIdentifier - 0x22: với DID đọc VIN - 0x22 F1 90
- Request:
<img width="1547" height="1409" alt="image" src="https://github.com/user-attachments/assets/5e4ba2b4-d7ec-4dec-a22a-7af79687fb27" />

<img width="2564" height="1411" alt="image" src="https://github.com/user-attachments/assets/5e84e76e-5e65-4c67-97ff-de0795f57442" />



- Response:
  + Nhận FF:

<img width="1523" height="1406" alt="image" src="https://github.com/user-attachments/assets/a09af863-eadb-4afc-a39e-b0e9da328bda" />


  + Nhận đầy đủ các CF:

<img width="1402" height="1411" alt="image" src="https://github.com/user-attachments/assets/28add504-a1f1-493a-8bd9-ccb3be5b2f57" />

<img width="1425" height="1412" alt="image" src="https://github.com/user-attachments/assets/77d5cd58-09e7-4170-ae8b-ba9b9d0c4852" />

### b2. Service: ReadDataByIdentifier - 0x22: với DID đọc tốc độ Engine - 0x22 01 0C
- Request:

<img width="2564" height="1410" alt="image" src="https://github.com/user-attachments/assets/6642cfc4-3e52-4a58-b885-74548feafc6b" />

- Response: RPM Engine giả lập đang là 3000

<img width="1658" height="1409" alt="image" src="https://github.com/user-attachments/assets/9356d629-6391-4082-9401-9abd3d336766" />

### b3. Service: ReadDataByIdentifier - 0x22: với DID đọc nhiệt độ Engine - 0x22 01 05
- Request:

<img width="2564" height="1406" alt="image" src="https://github.com/user-attachments/assets/f5466b0c-eed9-466f-a072-403529465ab3" />

- Response: Temperature Engine đang giả lập từ COM là 90 (0x5a)

<img width="1613" height="1406" alt="image" src="https://github.com/user-attachments/assets/99a66251-81c8-4615-9ac9-9e1dc8aea604" />

### b4. Service: ReadDTCInformation - 0x19 02 FF:

- Request:

<img width="2564" height="1407" alt="image" src="https://github.com/user-attachments/assets/c821253d-4564-4218-aad7-f3e616fd7ffa" />

- Response: DTC giả lập đang được response từ ECU khách hàng.


<img width="1551" height="1409" alt="image" src="https://github.com/user-attachments/assets/d1bffc26-a478-4082-bf89-69b46350dd2c" />

-> Nhận các CF tiếp theo:

<img width="1720" height="1407" alt="image" src="https://github.com/user-attachments/assets/d302f407-31dc-44f2-b152-71c74aa87fec" />

<img width="1534" height="1406" alt="image" src="https://github.com/user-attachments/assets/8c58ba8b-715a-48d6-a50b-8999c1a848f4" />

<img width="1517" height="1407" alt="image" src="https://github.com/user-attachments/assets/701c9217-d93c-47f4-955e-81409c67e6b6" />


### 5.2: Đối với tín hiệu Engine status: Signal packed trace bằng breakpoint đến Driver thực tế sẽ được nạp vào mailbox của STM32F103
- Các Signal được đóng gói và cập nhật theo chu kỳ 100ms cho ECU với giả định như sau:
byte 0 byte 1: Engine RPM = 3000
byte 2: Engine Temp = 90
byte 3: Engine_TorqueActual = 100Nm
byte 4: Engine State: 2_running
byte 5: low nibble: alive counter
        high nibble: CRC
byte 6, byte 7: reserved

![alt text](image.png)




## Author
* Dang Hoang Duy
* Email: danghoangduy.ltp@gmail.com | (+84) 328 880 024
