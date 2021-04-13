#   上海交大操作系統chcore-lab1：实验启动
&emsp;&emsp;这是上海交通大学的操作系统课程，代码获取地址可以参考[这里](https://ipads.se.sjtu.edu.cn/mospi/)，配套书本是《现代操作系统：原理与实现》，适合入门。  
&emsp;&emsp;lab1主要是bootloader的实现，包括开机，从实模式跳转到保护模式，加载内核代码等等，给后面的实验打下基石，下面简单介绍lab1中的几个练习。

##  实验平台搭建
&emsp;&emsp;实验需要依赖的代码还是挺多的，官网上也提供了配置好环境的虚拟机，不过不知道为什么我下载不了，所以我就自己配。具体配置的过程不赘述，简单来说就是缺什么补什么，很快就配置好了。

##  练习3
    结合readelf -S build/kernel.img读取符号表与练习2 中的GDB 调试信息，请找出请找出build/kernel.image入口定义在哪个文件中。继续借助单步调试追踪程序的执行过程，思考一个问题：目前本实验中支持的内核是单核版本的内核，然而在Raspi3 上电后，所有处理器会同时启动。结合boot/start.S中的启动代码，并说明挂起其他处理器的控制流。

&emsp;&emsp;想要找到入口定义在哪里，只需要打开gdb，然后输入where就能知道初始启动函数。具体方法是，一个窗口输入`make qemu-gdb`，一个窗口输入`make gdb`，然后在gdb的窗口输入`where`，就能看到当前的函数名字，由于挂在了gdb，所以此时qemu处于第一行代码尚未执行的状态，所以此时所在的函数就是入口所在的函数。    

    (gdb) where
    #0  0x0000000000080000 in _start ()
    Backtrace stopped: not enough registers or memory available to unwind further

&emsp;&emsp;想要知道不同处理器的执行逻辑，首先我们需要确定的是，一共有多少个处理器。在gdb的窗口内输入`info threads`，可以看到以下输出  

    (gdb) info threads  
    Id   Target Id         Frame   
    * 1    Thread 1.1 (CPU#0 [running]) 0x0000000000080000 in _start ()  
    2    Thread 1.2 (CPU#1 [running]) 0x0000000000080000 in _start ()  
    3    Thread 1.3 (CPU#2 [running]) 0x0000000000080000 in _start ()  
    4    Thread 1.4 (CPU#3 [running]) 0x0000000000080000 in _start ()  

&emsp;&emsp;很明显，在booloader启动时，一共有4个处理器（这里用线程来假装是处理器），并且此时运行的是处理器1。  
&emsp;&emsp;第二个问题是，由于这是单核版本，那么，这个4个处理器中哪个会运行？这里我们需要看一下执行逻辑，怎么看呢？只能从代码里看，由于上面我们知道了，程序入口是_start函数，于是我们反汇编_start试试。当然，由于我们有相应的汇编代码，所以也可以直接打开`/boot/start.S`查看，下面展示的是程序开头的部分反汇编代码:  

    =>0x0000000000080000 <+0>:     mrs     x8, mpidr_el1  
    0x0000000000080004 <+4>:     and     x8, x8, #0xff  
    0x0000000000080008 <+8>:     cbz     x8, 0x80010 <_start+16>  
    0x000000000008000c <+12>:    bl      0x8000c <_start+12>   
    0x0000000000080010 <+16>:    bl      0x87000 <arm64_elX_to_el1>  

&emsp;&emsp;从程序开头的逻辑可以看出，对于每个处理器，会先对自己的处理器编号进行与操作，并且只有与操作结果为0的处理器可以跳转到0x0000000000080010处执行下一步操作；而与操作结果非0的处理器则会一直0x000000000008000c在这条指令处做死循环。所以很明显了，程序启动后，只有cpu0(也就是上面的thread 1.1)会执行bootloader和加载启动内核，其他处理器处于空耗状态。  

##  练习4
    查看build/kernel.img的objdump信息。比较每一个段中的VMA 和LMA 是否相同，为什么？在VMA 和LMA 不同的情况下，内核是如何将该段的地址从LMA 变为VMA？提示：从每一个段的加载和运行情况进行分析。  

&emsp;&emsp;在命令行中使用objdump,可以得到kernel.img内各段的信息:  

    sworduo@ubuntu:~/Documents/Course/Book/chcore-lab$ objdump -h build/kernel.img  
    build/kernel.img:     file format elf64-little  
    Sections:  
    Idx Name          Size      VMA               LMA               File off  Algn
    0 init          0000b5b0  0000000000080000  0000000000080000  00010000  2**12  
                  CONTENTS, ALLOC, LOAD, CODE  
    1 .text         00001de0  ffffff000008c000  000000000008c000  0001c000  2**3  
                  CONTENTS, ALLOC, LOAD, READONLY, CODE  
    2 .rodata       00000114  ffffff0000090000  0000000000090000  00020000  2**3  
                  CONTENTS, ALLOC, LOAD, READONLY, DATA  
    3 .bss          00008000  ffffff0000090120  0000000000090120  00020114  2**4  
                  ALLOC  
    4 .comment      00000032  0000000000000000  0000000000000000  0002149d  2**0  
                  CONTENTS, READONLY  

&emsp;&emsp;在分析之前我们首先需要明确LMA和VMA的定义，LMA指的是程序加载进内存的地址，VMA指的是程序开始运行的地址，也就是程序从内存哪里开始运行。一般来说LMA=VMA，也就是说，程序加载进内存哪里，就从哪里开始。但是也有例外。比如说bootloader加载操作系统的elf里的各个段，由于一开始在实模式下，计算机访问的内存空间受限，所以只能将所有段放在内存的低地址空间处，也就是LMA是低地址内存空间。但是bootloader跳转后，从实模式转为保护模式，此时计算机可以访问整个内存空间，根据操作系统的知识，内核代码放在内存空间的高地址处，所以内核应该从高地址空间开始运行，所以此时需要将内核代码从低地址内存空间映射到高地址内存空间，也就是VMA是高地址内存空间。此时LMA和VMA就不一样了。  
&emsp;&emsp;也就是说，此时内核实际放在内存里的某个地方（LMA），但是逻辑映射上（VMA)是处于高地址空间。为什么其他程序不会这样呢？个人觉得，一家之言，不保真。这是因为其他程序运行时，就已经启动了虚拟内存地址，此时程序装载的地方由虚拟内存地址决定，所以LMA=VMA。而内核代码装载进内存是在虚拟地址机制启动前，内核代码运行却是在虚拟地址机制启动后，此时内核代码放在哪里由自己决定，而逻辑上从哪里开始运行，则有虚拟地址决定，所以会出现LMA不等于VMA的情况。  
&emsp;&emsp;回到上面得到的段信息，init存放的bootloader的代码，所以一直在低地址空间，VMA=LMA;而text等段是内核有关的代码，被映射到高地址空间，所以此时VMA不等于LMA。  

##  练习5
    以不同的进制打印数字的功能（例如8、10、16） 尚未实现， 请在kernel/common/printk.c中填充printk_write_num以完善printk的功能。

&emsp;&emsp;比较常规的进制转换问题，直接实现即可。

```c
static int printk_write_num(char **out, long long i, int base, int sign,
			    int width, int flags, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	char *s;
	int t, neg = 0, pc = 0;
	//这里将有符号数转为无符号数,如果i是负数,那么此时会变成很大的正数
	unsigned long long u = i;

	if (i == 0) {
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints(out, print_buf, width, flags);
	}

	if (sign && base == 10 && i < 0) {
		neg = 1;
		//将负数变成正数,从很大的正数(比如-1对应的无符号数)变成对应的绝对值(也就是1)
		u = -i;
	}
	// TODO: fill your code here
	// store the digitals in the buffer `print_buf`:
	// 1. the last postion of this buffer must be '\0'
	// 2. the format is only decided by `base` and `letbase` here

	s = print_buf + PRINT_BUF_LEN;
	*--s = '\0';
	while(u > 0){
		s--;
		t = u % base;
		if(t < 10){
			*s = t + '0';
		}
		else{
			*s = t - 10 + letbase;
		}
		u /= base;
	}
	//这里有个小问题，转换的数字长度可能会长于print_buf_len-1，s可能会超过系统分配给他的范围，不过暂时没问题就先不改了。
	//可能还存在一个问题，如果base特别大，比如10000，那么可能t的值会超过一个字符的大小，然后出错。不过这里是static函数，只要保证外层调用时只提供某些进制的转换即可。

	if (neg) {
		if (width && (flags & PAD_ZERO)) {
			simple_outputchar(out, '-');
			++pc;
			--width;
		} else {
			*--s = '-';
		}
	}
	return pc + prints(out, s, width, flags);
}
```

&emsp;&emsp;这里要注意的是，打印时先打印的是高位数字，但是计算时首先计算的是低位数字，所以可以直接将指针移到数组末尾，从后往前保存计算结果即可。  

##  练习6
    内核栈初始化（即初始化SP 和FP）的代码位于哪个函数？内核栈在内存中位于哪里？内核如何为栈保留空间？

&emsp;&emsp;在`/boot/start.S`中有如下代码：  
```
	/* Prepare stack pointer and jump to C. */
	adr 	x0, boot_cpu_stack
	add 	x0, x0, #0x1000
	mov 	sp, x0
```
&emsp;&emsp;可以看到程序首先加载`boot_cpu_stack`这个变量的地址，然后将偏移了4096字节的地址赋值给了sp，完成了栈指针的初始化。为什么要偏移？因为栈是从高地址向低地址扩展，这4096字节就是留给内核栈的空间。  
&emsp;&emsp;而变量`boot_cpu_stack`则是在`/boot/init_c.c`中定义（可以通过gdb查看），这是一个定义好的`4*4096`大小的二维全局数组，每个CPU使用其中一维。  
```c
char boot_cpu_stack[PLAT_CPU_NUMBER][INIT_STACK_SIZE] ALIGN(16);
```

##  练习7/8
    练习7：为了熟悉AArch64 上的函数调用惯例，请在kernel/main.c中通过GDB 找到stack_test函数的地址，在该处设置一个断点，并检查在内核启动后的每次调用情况。每个stack_test递归嵌套级别将多少个64位值压入堆栈，这些值是什么含义？提示：GDB可以将寄存器打印为地址及其64位值或数组，例如  
    (gdb) x/g $x29  
    0xffffff000020f330 <kernel_stack+7984>: 0xffffff000020f350  
    (gdb) x/10g $x29  
        0xffffff000020f330 <kernel_stack+7984>: 0xffffff000020f350 0xffffff00000d009c  
        0xffffff000020f340 <kernel_stack+8000>: 0x0000000000000001 0x000000000000003e  
        0xffffff000020f350 <kernel_stack+8016>: 0xffffff000020f370 0xffffff00000d009c  
        0xffffff000020f360 <kernel_stack+8032>: 0x0000000000000002 0x000000000000003e  
        0xffffff000020f370 <kernel_stack+8048>: 0xffffff000020f390 0xffffff00000d009c  

    练习8：在AArch64 中，返回地址（保存在x30寄存器），帧指针（保存在x29寄存器）和参数由寄存器传递。但是，当调用者函数（caller function）调用被调用者函数（callee fcuntion）时，为了复用这些寄存器，这些寄存器中原来的值是如何被存在栈中的？请使用示意图表示，回溯函数所需的信息（如SP、FP、LR、参数、部分寄存器值等）在栈中具体保存的位置在哪？  

&emsp;&emsp;`x29`存放的是当前sp指针的地址，命令`x/g $x29`是指将寄存器`x29`的地址及其内容展示出来，而命令`x/10g $29`，则是将以寄存器`x29`地址为起点的连续10个内容展示出来（每个内容都是64位），也就是离栈顶最近的十个地址的内容。然而在我的电脑上，直接输入这些命令会使用十进制展示这些内容，不太方便，所以我做实验时输入的是这些命令`x/10xg $29`，以十六进制打印结果。  
&emsp;&emsp;我们首先在stack_test和stack_backtrace两个函数处打下断点，然后键入`c`多次运行程序，直到遇到断点2(也就是打在stack_backtrace处的断点)停止，然后输入`x/10xg $x29`查看此时栈内的内容。  

    (gdb) b stack_test  
    Breakpoint 1 at 0xffffff000008c030: file ../kernel/main.c, line 27.  
    (gdb) b stack_backtrace   
    Breakpoint 2 at 0xffffff000008c0dc: file ../kernel/monitor.c, line 30.    
    (gdb) c  
    Continuing.  
    Thread 1 hit Breakpoint 1, stack_test (x=5) at ../kernel/main.c:27  
    27      ../kernel/main.c: No such file or directory.  
    (gdb) c  
    Continuing.  
    Thread 1 hit Breakpoint 1, stack_test (x=4) at ../kernel/main.c:27  
    27      in ../kernel/main.c  
    (gdb) c  
    Continuing.  
    Thread 1 hit Breakpoint 1, stack_test (x=3) at ../kernel/main.c:27  
    27      in ../kernel/main.c  
    (gdb) c  
    Continuing.  
    Thread 1 hit Breakpoint 1, stack_test (x=2) at ../kernel/main.c:27  
    27      in ../kernel/main.c  
    (gdb) c  
    Continuing.  
    Thread 1 hit Breakpoint 1, stack_test (x=1) at ../kernel/main.c:27  
    27      in ../kernel/main.c  
    (gdb) c  
    Continuing.  
    Thread 1 hit Breakpoint 1, stack_test (x=0) at ../kernel/main.c:27  
    27      in ../kernel/main.c  
    (gdb) c  
    Continuing.  
    Thread 1 hit Breakpoint 2, stack_backtrace () at ../kernel/monitor.c:30  
    30      ../kernel/monitor.c: No such file or directory.  
    (gdb) x/10xg $x29  
    0xffffff0000092040 <kernel_stack+7968>: 0xffffff0000092060      0xffffff000008c070  
    0xffffff0000092050 <kernel_stack+7984>: 0x0000000000000001      0x00000000ffffffc0 

    0xffffff0000092060 <kernel_stack+8000>: 0xffffff0000092080      0xffffff000008c070  
    0xffffff0000092070 <kernel_stack+8016>: 0x0000000000000002      0x00000000ffffffc0 

    0xffffff0000092080 <kernel_stack+8032>: 0xffffff00000920a0      0xffffff000008c070  
    0xffffff0000092090 <kernel_stack+8048>: 0x0000000000000003      0x00000000ffffffc0 

    0xffffff00000920a0 <kernel_stack+8064>: 0xffffff00000920c0      0xffffff000008c070  
    0xffffff00000920b0 <kernel_stack+8080>: 0x0000000000000004      0x00000000ffffffc0 

    0xffffff00000920c0 <kernel_stack+8096>: 0xffffff00000920e0      0xffffff000008c070  
    0xffffff00000920d0 <kernel_stack+8112>: 0x0000000000000005      0x00000000ffffffc0  

&emsp;&emsp;为了方便查看，我将不同函数调用的函数栈分割开来。可以看到对于`stack_test`函数来说，每次调用的函数栈大小都是32，32是怎么确定的呢，我们可以通过反汇编该函数来查看。

    (gdb) disas stack_test  
    Dump of assembler code for function stack_test:  
    分配函数栈空间  
    0xffffff000008c020 <+0>:     stp     x29, x30, [sp,#-32]!    
    0xffffff000008c024 <+4>:     mov     x29, sp    
    0xffffff000008c028 <+8>:     str     x19, [sp,#16]  
    0xffffff000008c02c <+12>:    mov     x19, x0  
    ...根据参数判断下一步是递归还是回溯  
    0xffffff000008c040 <+32>:    cmp     x19, #0x0  
    0xffffff000008c044 <+36>:    b.gt    0xffffff000008c068 <stack_test+72>  
    0xffffff000008c048 <+40>:    bl      0xffffff000008c0d0 <stack_backtrace>  
    ...函数递归的逻辑  
    0xffffff000008c068 <+72>:    sub     x0, x19, #0x1  
    0xffffff000008c06c <+76>:    bl      0xffffff000008c020 <stack_test>  
    0xffffff000008c070 <+80>:    b       0xffffff000008c04c <stack_test+44>  

&emsp;&emsp;上面只保留了和分析有关的代码。可以看到，`stack_test`函数进来时首先会将sp指针减32，这意味着这个函数预留的函数栈大小就是32。  
&emsp;&emsp;仔细分析`x/10xg $x29`的结果，由于栈是从高地址向低地址延伸，假设函数调用了五次，那么从上到下（也就是栈地址从低到高）的函数栈依次对应第五次、第四次、第三次、第二次、第一次函数调用的函数栈。由于该函数的函数栈只有32（对应于十六进制的20），并且每个函数栈至少需要保存上一个函数的FP地址和该函数的返回地址，通过比对得知，每个函数栈的栈帧里，第一个八位存放的是上一个函数的FP地址（用于函数回溯，存放的是上一个函数的sp起始地址），第二个八位存放的是当前函数的返回地址LR（用于函数返回，可以看到stack_test函数内部递归stack_test后，下一条指令的地址就是0xffffff000008c070），第三个八位和第四个八位未知作用未知。下图展示了一般情况下的栈帧结构（图片来自[这里](https://zhuanlan.zhihu.com/p/360873948))：  

![stack_architecture](https://github.com/sworduo/Course/tree/master/pic/chcore/lab1/stack_architecture.jpg "stack_architecture")  

&emsp;&emsp;如果有朋友和我一样，直接输入`b stack_test`会报以下错误：  
```
(gdb) disas stack_test
Dump of assembler code for function stack_test:
   0xffffff000008c020 <+0>:     Cannot access memory at address 0xffffff000008c020
```
&emsp;&emsp;解决方法很简单，这个错误的形成可能是当前处于实模式状态，所以无法跳转到高地址空间，所以只需要从实模式切换到保护模式，就能在这个函数出添加断点。  
&emsp;&emsp;如何判断哪个代码是从实模式切换到保护模式？有两种方式，第一是直接读代码；第二是gdb调试。我这里用的是第二种方法，依据是看代码的地址什么时候出现高位地址，或者什么时候进入main函数，简单看了下代码，`init_c.c`还是处于实模式状态，所以可以`disas init_c`反汇编这个函数，找到该函数的最后一条指令的地址，在这里设置一个断点，点击`c`运行到此处。接下来只需要`si 4`运行几个指令，就跳到保护模式，这个时候就能在函数`stact_test`处设置断点了。

##  练习9
    使用与示例相同的格式， 在kernel/monitor.c中实现stack_backtrace。为了忽略编译器优化等级的影响，只需要考虑stack_test的情况，我们已经强制了这个函数编译优化等级。挑战：请思考，如果考虑更多情况（例如，多个参数）时，应当如何进行回溯操作？  
    示例：  
    Stack backtrace:
    LR ffffff00000d009c FP ffffff000020f330 Args 0 0 ffffff000020f350 ffffff00000d009c 1  
    LR ffffff00000d009c FP ffffff000020f350 Args 1 3e ffffff000020f370 ffffff00000d009c 2  
    LR ffffff00000d009c FP ffffff000020f370 Args 2 3e ffffff000020f390 ffffff00000d009c 3  

&emsp;&emsp;首先我们要确认，具体的输出是什么？分析题意可知（看lab1的pdf），需要输出的是，LR=>上一个函数的返回地址（注意是上一个函数，不是这个函数），FP=>上一个函数的栈顶地址（FP），Args=>上一个函数的调用参数（注意是传进上一个函数的参数，不是传进这个函数的参数）。由上面的分析可知，上一个函数的FP保存在当前函数的的FP栈顶位置，由于每个函数的返回地址都保存在栈顶位置+8处，所以上一个函数的返回地址就是上一个函数FP+8处。
&emsp;&emsp;有些绕的是上一个函数的调用参数，分析`stack_test`的反汇编代码可知，函数开始时会将寄存器`$19`存放入栈顶偏移16位置处，而`$19`是存放的是上一个函数里`x0`的值，也就是上一个函数里的参数，所以可知，上一个函数的调用参数存放在当前函数的栈顶偏移16位置处。  

```
(gdb) disas stack_test
Dump of assembler code for function stack_test:
   0xffffff000008c020 <+0>:     stp     x29, x30, [sp,#-32]!
   0xffffff000008c024 <+4>:     mov     x29, sp
   0xffffff000008c028 <+8>:     str     x19, [sp,#16]
   0xffffff000008c02c <+12>:    mov     x19, x0
    ...
   0xffffff000008c068 <+72>:    sub     x0, x19, #0x1
   0xffffff000008c06c <+76>:    bl      0xffffff000008c020 <stack_test>
   0xffffff000008c070 <+80>:    b       0xffffff000008c04c <stack_test+44>
```

&emsp;&emsp;通过上面的分析，在获取了当前栈顶的位置后，可以通过指针偏移来获得对应的FP、LR、Args的值。不过需要注意的是，函数`read_fp`返回的虽然是个`u64`，但其实质上代表的是栈顶的地址，所以需要将其转换为指针。同理，LR和FP都是指针，需要在解引用后将其重新转换为指针类型，而Args则是数字，只需要对指针解引用即可。  
&emsp;&emsp;由于函数回溯时以栈顶的内容为0作为结束条件，也就是说，函数回溯会一直回溯到程序起始的第一个函数处。  
&emsp;&emsp;这里的`(*u64)`类型的长度就是8位，所以只需要对当前指针+1，就能实现8位的偏移。可以类比`int`指针，`int*`指针+1事实上是偏移了`int`的长度，也就是4位。  

```c
__attribute__ ((optimize("O1")))
int stack_backtrace()
{
	printk("Stack backtrace:\n");

	// Your code here.
	//回溯会回溯到fp的值为0的情况，也就是整个程序最开始的函数那里
	//注意fp是当前函数的栈的栈顶，这是个地址！里面的值才是我们想要的
	u64* current_fp = (u64*)read_fp();
	// printk("%lx %lx\n", current_fp, *current_fp);
	while(*current_fp){
		u64* lastfp = (u64*)*current_fp;
		u64* lr = (u64*)*((u64*)(lastfp+1));
		u64 arg = *(u64*)(current_fp+2);
		printk("LR %lx FP %lx Args %lx\n", lr, lastfp, arg);
		current_fp = lastfp;
	}
	return 0;
}
```

##  总结
&emsp;&emsp;lab1主要的工作就是bootloader和加载内核代码，是整个操作系统的基石，虽然代码量不大，但是值得反复思考和分析。接下来的实验会更加贴近操作系统课本上的知识，比如内存管理、线程切换、文件管理等等，这些才是重头戏。  








