## cpc
- 2021.7.1 突发奇想 “建党 100 周年！写一个名为 CPC 的 C 编译器，为党庆生！”

## 实现方式
1. C代码 tokenize -> symbol table
2. parse & generate -> VM instructions
3. run VM & 执行生成好的 instructions

> 思路来自于著名的 C4，一个 500 行的可自举的 C 编译器.   
> 我这个行数目测要超过 1000 行，anyway，just for fun.
