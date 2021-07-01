# cpc
2021.7.1 突发奇想
“建党 100 周年！写一个名为 CPC 的 C 编译器，为党庆生！”

实现方式：C代码 tokenize -> 符号表 parse -> AST generate -> VM 指令集 -> run by vm

实现思路来自于著名的 C4，一个 500 行的可自举的 C 编译器
我这个行数目测要超过 2000 行，anyway，just for fun.
