# 3 利用可见光传输信息的软件

[TOC]

## 实验目的

​	通过完成实验， 理解物理层传输的基本原理。掌握传输过程中的编解码过程，熟悉传输中的噪声、分辨率、波特率、调制和误码等通信概念；了解奈氏定理和香农定理的含义。  

## 使用说明

​	使用bin目录下的encode/decode皆可（只是用于IDE内debug）（注：以下都是相对路径）
​	对于编码，传入格式为"encode <输入文件路径> <输出文件路径> <生成视频时长（毫秒）>"
​	对于解码，传入格式为"decode <输入视频路径> <输出文件路径> <解码信息路径> <（可选）原文件路径>"（注：输入原文件路径后解码信息才有效）

## 过程概述

![cni-exp_E1_3790](E:\code\practice\Computer_Network\Experiment\cni-exp\E1_3790\doc\cni-exp_E1_3790.png)

首先读入data.bin文件；
通过视频时长和数据量来计算每张二维码的数据量，以此进行分块；
然后把分块数据装在结构体qrData中；
将结构体数据序列化成字符数组；
将字符数据编码为二维码，写入Mat中，再用opencv写出到jpg中；
调用ffmpeg命令行指令将图片合成为视频；

手机拍摄；

读入video.mp4；
调用ffmpeg命令行指令将视频分解为图片；
识别二维码解码为字符，再反序列化为结构体数据；
再把结构体中的文件数据进行合成，得到解码后的数据，输出到decoded.bin文件；
和原文件进行对比，结果输出到info.bin文件；

## 算法细节和相关指标

1. 在实验中，编解码算法是什么？
   编码算法是依赖于QR Code标准编码，通过选择不同的编码模式，使用Reed-Solomon纠错码纠错，编码生成二维码。此处使用libqrencode库进行编码。
   解码算法也是依赖于QR Code标准来进行解码。此处使用zbar库进行识别、解码。
2. 在实验中的，调制和解调算法是什么？其中，载体信号、调制信号是什么？使用的算法属于调频、调幅还是调相？
   这里的调试是计算机通过显示器将视频文件调制为光信号；而解调是手机摄像头接收光信号后将其转化为电子信号储存在视频中。
   载体信号是光，调制信号是视频文件的电子信号。
   使用的算法属于调幅，因为它是通过控制每个像素的红绿蓝信号的高低传输光信号的。
3. 在实验中的，主要的噪音强度有多大，噪音来自哪些因素？
   选取生成的二维码和其对应的视频的一帧，计算得到信噪比约为2，ssim约为0.6，可见噪音强度相当大。
   <img src="C:\Users\WQC20\AppData\Roaming\Typora\typora-user-images\image-20240922184712434.png" alt="image-20240922184712434" style="zoom:50%;" />
   一个是手机拍摄时光信号传输过程中，可能有其他光线的干扰，物理介质如屏幕、空气、摄像头在传输过程中不能完整地传输信号，以及可能有其他电气信号的干扰。
4. 你的编码算法分辨率是多少？
   libqrencode编码模式为0自动选择，按项目中的示例来讲，是93x93。
5. 你的编码波特率是多少？传输率是多少？
   项目设计的是按照视频长度控制每张二维码的信息量，按项目中的data.bin和10秒的视频，每张是2584B，即20,672bit。
   视频默认传输为10帧，那么传输率是20672 * 10帧/秒 = 206720 bit/s = 206.72 kbps
6. 按奈氏定理和香农定理，通信率上限是多少？
   奈氏定理：$C = 2Blog_2M$，B = 206.72kbps，M = 2，C = 2 * 206.72 * 1.414 = 584.60416。
   香农定理：$C = 2Blog_2(1 + \frac{S}{N})$，B = 206.72kbps，$\frac{S}{N} = 2.2682$，C = 212.63576。

## 主要代码

### main函数

 <img src="C:\Users\WQC20\Desktop\vkvIu31.png" alt="vkvIu31" style="zoom:67%;" /> 

### encode&decode

 ![XoIofM1](C:\Users\WQC20\Desktop\XoIofM1.png)

### 调用ffmpeg

<img src="C:\Users\WQC20\AppData\Roaming\Typora\typora-user-images\image-20240922200848722.png" alt="image-20240922200848722" style="zoom:67%;" /> 

## 一些遇到的问题

### opencv不能识别某些二维码

​	解决方法一：使用ZBar库代替opencv，准确率高一点。

​	解决方法二：将二维码的数据量限制在较小的范围。

​	解决方法三：使用另一个库进行二维码编码（但似乎效果一般）。

​	废案：在编码二维码的时候就对该二维码尝试进行解码，如果不行就重新生成或使用其他随机数据生成。不使用的原因一是耗费时间很长，二是效果和在解码时进行该操作一致。

### 关于编码方式

​	一开始我是还使用了base64编码，但似乎这种编码和实验目的不符，他只是进行了加密或者是为了适配传输设备等的一种编码，也不具备查错纠错能力。

### 视频分出的帧的图片无法被识别

​	解决方法：把这一段数据替换为任意等长的数据。

### 由于手机录像和视频的帧率不一，会有重复二维码或跳过二维码的情况

​	解决方法：使用qrData结构体储存，记录每个二维码的序号，记录上一帧的图像，对于重复的二维码直接跳过，对于跳过的二维码使用任意等长的数据补上（无法被识别也包括在里面）。

## 运行截图

​	<img src="C:\Users\WQC20\AppData\Roaming\Typora\typora-user-images\image-20240922200347682.png" alt="image-20240922200347682" style="zoom:67%;" /> 

​	<img src="C:\Users\WQC20\AppData\Roaming\Typora\typora-user-images\image-20240922200422124.png" alt="image-20240922200422124" style="zoom:67%;" /> 

​	<img src="C:\Users\WQC20\AppData\Roaming\Typora\typora-user-images\image-20240922200529236.png" alt="image-20240922200529236" style="zoom:67%;" />  

​	<img src="C:\Users\WQC20\AppData\Roaming\Typora\typora-user-images\image-20240922200657893.png" alt="image-20240922200657893" style="zoom:67%;" /> 