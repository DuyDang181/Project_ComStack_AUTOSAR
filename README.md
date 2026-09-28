# Project_ComStack_AUTOSAR
Mục tiêu của dự án là nghiên cứu và phát triển ECU dựa trên kiến trúc Layered - AUTOSAR Classic Platform với module COM. Mô tả Data Path qua từng Layer từ đó phát triển các dịch vụ Diagnostic gửi các Request chẩn đoán đến eVCU của Khách hàng. 
Dự án phát triển ECU dựa trên kiến trúc Microcontroller STM32F103C8T6 Bulepill
> Đây là dự án nghiên cứu và học tập, phát triển Porfolio cá nhân. Dự án sẽ không tuân thủ đầy đủ các Standard trong kiến trúc AUTOSAR Classic Platform cũng như ISO 26262,...


## Mục lục
1. [Mô hình dự án](#1-mô-hình-dự-án)
2. [Kiến trúc hệ thống](#2-kiến-trúc-hệ-thống)
3. [Công cụ & Môi trường phát triển](#3-công-cụ--môi-trường-phát-triển)
4. [Kiểm thử và xác minh](#4-kiểm-thử-và-xác-minh)
5. [Kết quả](#5-kết-quả)
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

## 5. Kết quả_ Debug


## Author
* Dang Hoang Duy
* Email: danghoangduy.ltp@gmail.com | (+84) 328 880 024
