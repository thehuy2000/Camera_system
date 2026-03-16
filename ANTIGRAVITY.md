# ANTIGRAVITY.md — AI Control Panel

> File này là entry point cho AI. Đọc file này TRƯỚC KHI đọc bất kỳ thứ gì khác trong project.

## Project Overview

- **Tên**: cam_system 
- **Mục đích**: Project đọc dữ liệu từ camera, xử lý và ghi vào file.
- **Ngôn ngữ tài liệu**: Tiếng Việt


## Thứ tự đọc (Learning Order)

Khi tiếp cận project này, AI PHẢI đọc theo thứ tự:

1. `ANTIGRAVY.md` (file này) — rules, constraints, context
2. `core/` — Module nền tảng của project
3. `include/` — Header file của project
4. `src/` — Source code của project
5. `tests/` — Test cases của project
6. `Makefile` — Makefile của project
7. `scripts/` — Scripts của project
8. `conversations/` — Lịch sử thảo luận (nếu cần thêm context)

> **Nguyên tắc**: Code là source of truth. Khi document và code mâu thuẫn → tin code.

## Rules

### Quy tắc chung

- **Explicit > Implicit**: Không đoán. Nếu không rõ, hỏi lại.
- **Rationale matters**: Mọi quyết định phải kèm lý do (WHY, không chỉ WHAT).
- **Text-first**: Dùng Markdown, Mermaid cho tài liệu. Không dùng binary formats.
- **Không tự ý thêm feature** ngoài scope được yêu cầu.
- **Commit messages**: Viết bằng tiếng Việt.

### Quy tắc code (C / Kernel)

- Tuân thủ [Linux Kernel Coding Style](https://www.kernel.org/doc/html/latest/process/coding-style.html).
- Indent bằng TAB (không dùng spaces).
- Tên biến, hàm: `snake_case`.
- Tên macro: `UPPER_SNAKE_CASE`.
- Mỗi function tối đa ~50 dòng. Nếu dài hơn, tách ra.
- Comment giải thích WHY, không giải thích WHAT (code phải tự giải thích WHAT).
- Luôn kiểm tra return value của các hàm có thể fail (kmalloc, copy_from_user, ...).

### Constraints đặc thù System Programming

AI **KHÔNG THỂ** compile hay chạy kernel code. Workflow bắt buộc:

1. **Human** chạy/debug trên máy thật → cung cấp output (dmesg, stack trace, error log)
2. **AI** phân tích output → đề xuất fix/giải pháp
3. **Human** verify và apply

Khi cần debug, AI phải yêu cầu human cung cấp:
- `dmesg` output
- Stack trace (nếu có)
- Steps to reproduce
- Kernel version (`uname -r`)

## Cấu trúc Project

```
├── ANTIGRAVY.md
├── conversations\
├── include\
│   ├── capture.h
│   ├── logger.h
│   ├── mem_pool.h
│   └── ring_buff.h
├── Makefile
├── output\
├── REQUIREMENT.md
├── src\
│   ├── app\
│   ├── core\
│   │   ├── logger.c
│   │   ├── mem_pool.c
│   │   └── ring_buff.c
│   ├── hardware\
│   └── main.c
└── tests\
    ├── test_mem_pool.c
    └── test_ring_buff.c

```

## Conversation Logs

AI PHẢI lưu lại cuộc hội thoại vào `conversations/`.

- **Đặt tên file**: `YYYY-MM-DD_mô-tả-ngắn.md`
- **Thời điểm lưu** (AI tự quyết định, tối thiểu ở các thời điểm sau):
  - Cuối session (trước khi kết thúc)
  - Khi có quyết định quan trọng (architecture, thay đổi rules)
  - Khi chuyển sang phase mới trong workflow
- **Nội dung**: Tóm tắt bối cảnh, các quyết định, lý do, và commits liên quan
- **Lưu ý**: AI chỉ tạo/cập nhật file. Việc commit conversation logs do human tự thực hiện.

## Workflow AI-First

1. **Brainstorm**: Thảo luận requirement với AI → lưu vào `conversations/`
2. **Vibe coding**: Prototype nhanh với AI (chất lượng thấp, tốc độ cao)
3. **Review**: Demo + review sequence diagram
4. **Production coding**: Code lại chuẩn — developer PHẢI hiểu từng dòng
5. **Testing**: Bàn giao docs + conversation logs cho tester
