- Ý tưởng: Parser sử dụng phương pháp phân tích cú pháp đệ quy (recursive descent parsing) để xử lý các token từ lexer, sau khi loại bỏ comment dạng // comment line  và /* comment block */. Nó xây dựng cây cú pháp (parse tree) dựa trên văn phạm của ngôn ngữ, đồng thời duy trì bảng ký hiệu (symbol table) để kiểm tra biến phải được khai báo trước khi sử dụng. Parser cũng thực hiện đồng bộ hóa lỗi (error recovery) để báo tối đa một lỗi mỗi dòng và tiếp tục phân tích sau khi gặp lỗi.
- Cách chạy:
	+ Tạo & chỉnh sửa file upl.c

	+ Biên dịch & chạy:
		++ Biên dịch file bằng câu lệnh: 
            gcc -o upl upl.c
		++ Chạy chương trình với file đầu vào: 
            ./upl input.txt 
		++ Nếu muốn xuất kết quả chạy ra file thì có thay bằng câu lệnh
		    		./upl input.txt > output.txt
    ++ Xem kết quả chạy :
				    cat output.txt
		++ Xem test đầu vào kèm kết quả chạy:
				    cat input.txt && cat output.txt
