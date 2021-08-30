## cpc
- 2021.7.1 突发奇想 “建党 100 周年！写一个名为 CPC 的 C 编译器，为党庆生！”

## 实现说明
> 思路来自于著名的 C4，一个 500 行的可自举的 C 编译器.   
> 我这个行数目测要超过 1000 行，anyway，just for fun.

最终，我只用了 700 行！GOODJOB！代码清晰易读，哈哈  
可以认为本项目是一个可读版的C4，基本没有原创成分

## 原理解析
原理解析已分章节做成视频上传 B 站了，标题为：「700行手写编译器」
 - Part 1:   [背景与设计思路](https://www.bilibili.com/video/BV1Kf4y1V783)
 - Part 2.1: [虚拟机与指令](https://www.bilibili.com/video/BV1Eq4y197B9)
 - Part 2.2: [栈与函数调用](https://www.bilibili.com/video/BV14U4y1J76i)
 - Part 3.1: [词法分析与符号表](https://www.bilibili.com/video/BV1hX4y1F7FD)
 - Part 3.2: [语法分析与递归下降](https://www.bilibili.com/video/BV1Lo4y1U7uv)
 - Part 3.3: [表达式与优先级爬山](https://www.bilibili.com/video/BV1T64y1v7jP)
 - Part 4:   [再看代码生成](https://www.bilibili.com/video/BV1iQ4y1h7HX)
