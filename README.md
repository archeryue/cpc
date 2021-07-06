## cpc
- 2021.7.1 突发奇想 “建党 100 周年！写一个名为 CPC 的 C 编译器，为党庆生！”

## 实现说明
1. C代码 tokenize -> symbol table
2. parse & generate -> VM instructions
3. run VM & 执行生成好的 instructions

> 思路来自于著名的 C4，一个 500 行的可自举的 C 编译器.   
> 我这个行数目测要超过 1000 行，anyway，just for fun.

最终，我只用了 700 行！GOODJOB！代码漂亮易读，哈哈。

## 原理解析
最核心的代码其实是两个函数：
- run_vm : 执行指令，依赖 init_vm
- parse  : 将 C 代码解析为指令，核心依赖 parse_fun/parse_stmt/parse_expr

#### VM 设计
##### 内存
虚拟机的内存包含三块空间
 - code // 存储指令（以及指令所需要的参数），pc 从 0 开始
 - data // 存储全局变量和常量，从小到大
 - stack // 栈空间，从大到小（由于这三块内存我们是分别存储的，所以stack并非一定要从大到小，只是习惯而已）

##### 寄存器
 - pc // 代码计数器，也就是 code 区域的指针，每步都是执行 pc 所指向的指令
 - sp // stack pointer, 栈指针
 - bp // base pointer，上一个栈的指针
 - ax // 通用寄存器，保存指令要用的数据，返回值也会保存在这里

##### 指令集
指令集一共有 4 大类
 - save & load：在寄存器和内存中间传递数据
   - IMM, LEA, PUSH, LC, LI, SC, SI
 - 四则运算、逻辑运算、位运算
   - OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD
 - 跳转、函数调用
   - JMP/JZ/JNZ, CALL/NVAR/RET/DARG
   - //TODO: 有空再细讲一下函数调用
 - Native CALL：虚拟机通过调用原生函数支持的一些指令（以方便处理动态内存、IO等问题，简化实现）
   - OPEN, READ, CLOS, PRTF, MALC, FREE, MSET, MCMP, EXIT

#### Parse 设计
Parse 最关键的就是要将 C 代码翻译成 VM 指令写入 code 空间（同时把全局变量和常量写入 data 空间） 
在 VM 运行之前，stack 空间应该是空的，而 code 和 data 应该是写好不需要再修改了的  
Parse 过程需要保存一些关键的临时数据，比如说我 Parse 过一个全局变量 x，后面我再遇到变量 x 的时候，
我需要准确的知道 x 的类型、内存地址，这样我才能正确的处理 x 相关的语句和表达式  
这个保存 Parse 过程临时数据的地方就是：符号表（symbol_table）
- 符号表 & 关键字
  - Token: 符号类型标识，默认是 Id，但是关键字和运算符都有自己的Id
  - Hash: just hash，用于快速判断两个 Id 是否相同
  - Name: just name, 符号的全名
  - Class: 符号的类别：Num(枚举符号), Fun(函数), Sys(Native Call), Glo(全局变量), Loc(局部变量)
  - Type: 符号所代表的变量或者函数的返回类型
  - Value: 符号的值，不同类别的符号值的含义不同，例如：全局变量的值为地址，Loc的值为顺序（地址在stack中，可根据顺序计算）
  - GClass, GType, GValue: 用于进入局部空间后，对同名全局符号的遮蔽时临时保存全局属性
- Parse 全局定义  
全局定义一共有两种：变量、函数，由于枚举变量以 enum 开头有些特殊，我们也把它单拎出来处理
  - parse 枚举：这个很容易 enum [Id] {Id [= Num], Id ....};
  - parse 变量：这个更容易 type[\*] Id [,Id] ... ;
  - parse 函数：这个麻烦些 type[\*] Id (type Id [, type Id] ... ) { 这里面是函数内部，单独讲 } 
- Parse 函数
  - 处理传入参数
  - 处理局部变量（遮蔽同名全局变量）
  - 一条一条处理 parse 语句
  - 恢复同名全局变量
- Parse 语句
  - If 语句
  > JZ false point  
  > true point: true stmts ...  
  > JMP end point  
  > false point: false stmts ...  
  > end point  
  - While 语句
  > loop point  
  > while stmt  
  > JZ end point  
  > stmts...  
  > JMP loop point  
  - Return 语句
  - 普通语句：直接当做表达式处理
- Parse 表达式  
表达式 Parse 我在经典的「递归下降」和 C4 使用的「优先级爬山」中犹豫了很久，最终使用简单的爬山法  
优先级爬山处理的方式也很优雅：
  - 遇到第一个 operator 直接压栈
  - 后续的 operator 与栈顶比较
    - 优先级更高的话直接处理
    - 优先级低的话则先处理栈中的 operator
  - 直到所有 operator 都处理完
