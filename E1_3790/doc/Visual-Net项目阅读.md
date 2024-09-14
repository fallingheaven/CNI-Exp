## 项目结构

```
Visual-Net/
├── bin/
│    ├── ffmpeg/ ffmpeg可执行文件
│    ├── decoder.exe
│    ├── encoder.exe
│    └── 其他dll文件
├── note/
│    ├── Code.md
│    ├── environment.md
│    └── TODO.md
├── src/
│    ├── ffmpeg/ 存放了ffmpeg的源代码
│    ├── include/
│    │	└── opencv2/ 存放了opencv的源代码
│    ├── lib/ 这里里面放了opencv的lib
│    ├── code.cpp code.h
│    ├── ffmpeg.cpp ffmpeg.h
│    ├── ImgDecode.cpp ImgDecode.h
│    ├── pic.cpp pic.h
│    ├── main.cpp
└── readme.md
```

## 代码分析

实际上重要的逻辑是在main.cpp里面，编码、解码、识别二维码都未必和项目一致。

### main.cpp

```c++
// argc和argv都通过命令行输入
int main(int argc, char* argv[])
{
    // type只能通过在代码中修改改变格式
	constexpr bool type = false;
	//type==true 将文件编码为视频  命令行参数 ： 输入文件路径 输出视频路径 最长视频时长
	//type==false 将视频编码为文件 命令行参数 ： 输入视频路径 输出图片路径
	if constexpr(type)
	{
		if (argc == 4)
			return FileToVideo(argv[1], argv[2], std::stoi(argv[3]));
		else if (argc == 5)
			return FileToVideo(argv[1], argv[2], std::stoi(argv[3]), std::stoi(argv[4]));
	}
	else
	{
		if (argc == 3)
			return VideoToFile(argv[1], argv[2]);
	}
	puts("argument error,please check your argument");
	return 1;
}
```



```c++
/// 图片转视频
/// \param filePath 输入图片文件的路径
/// \param videoPath 输出视频文件的路径
/// \param timLim 视频的时间限制
/// \param fps 视频的帧率
/// \return
int FileToVideo(const char* filePath, const char* videoPath,int timLim=INT_MAX,int fps=15)
{
    // 首先打开文件
	FILE* fp = fopen(filePath, "rb");
	if (fp == nullptr) return 1;
    
    // 将指针放到最后获取文件大小
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
    
    // 重置指针，读入内容
	rewind(fp);
	char* temp = (char*)malloc(sizeof(char) * size);
	if (temp == nullptr) return 1;
	fread(temp, 1, size, fp);
	fclose(fp);
    
    // 创建md outputImg目录，Code::Main将图像数据分割成帧，并将这些帧编码为二维码图像
    // FFMPEG::ImagetoVideo通过命令行调用ffmpeg将图片转成食品
	system("md outputImg");
	Code::Main(temp, size, "outputImg", "png",1LL*fps*timLim/1000);
	FFMPEG::ImagetoVideo("outputImg", "png", videoPath, fps, 60, 100000);
    // 命令行删除临时目录
	system("rd /s /q outputImg");
	free(temp);
	return 0;
}
```

```c++
/// 视频转图片
/// \param videoPath 输入视频文件的路径
/// \param filePath 输出文件的路径
/// \return
int VideoToFile(const char* videoPath, const char* filePath)
{
	char imgName[256];
    // 重建inputImg文件夹
	system("rd /s /q inputImg");
	system("md inputImg");
    
    // 启动线程，FFMPEG::VideotoImage命令行调用ffmpeg将视频转成图片
	bool isThreadOver = false;
	std::thread th([&] {FFMPEG::VideotoImage(videoPath, "inputImg", "jpg"); isThreadOver = true; });
    
	int precode = -1;
	std::vector<unsigned char> outputFile;
	bool hasStarted=0;
	bool ret = 0; // 错误标记
    
     // 循环中每次将处理过的图片用命令行删除
	for (int i = 1;; ++i, system((std::string("del ") + imgName).c_str()))
	{
		printf("Reading Image %05d.jpg\n",i);
		snprintf(imgName, 256, "inputImg\\%05d.jpg",i);
        
         // 读文件，保证读到文件以及进程已经处理完毕
		FILE* fp;
		do
		{
			fp = fopen(imgName, "rb");
		} while (fp == nullptr && !isThreadOver);

         // 错误处理
		if (fp == nullptr)
		{
			puts("failed to open the video, is the video Incomplete?");
			ret = 1;
			break;
		}
        
         // opencv读入图像文件
		cv::Mat srcImg = cv::imread(imgName, 1),disImg;
		fclose(fp);
		
         // 通过ImgParse::Main将srcImg变为二维码disImg，返回零成功，反之失败
		if (ImgParse::Main(srcImg, disImg))
		{
			continue;
		}
        
	    //Show_Img(disImg);
         // ImageDecode::Main解码二维码图像
		ImageDecode::ImageInfo imageInfo;
		bool ans = ImageDecode::Main(disImg, imageInfo);
         // 成功就没有后续
		if (ans)
		{
			continue;
		}
         
         // 起始帧标记
		if (!hasStarted)
		{
			if (imageInfo.IsStart)
				hasStarted = 1;
			else continue;
		}
         // 如果上一帧和这一阵的FrameBase相同，说明是重复帧
		if (precode == imageInfo.FrameBase) 
			continue;
         // 查看是否这一帧和前一帧的标号相差为1，若否，则除去上面重复情况就是跳帧了
		if (((precode + 1) & UINT16_MAX) != imageInfo.FrameBase)
		{
			puts("error, there is a skipped frame,there are some images parsed failed.");
			ret=1;
			break;
		}
         // 成功
		printf("Frame %d is parsed!\n", imageInfo.FrameBase);

		precode = (precode + 1) & UINT16_MAX;
         
         // 添加到输出
		for (auto& e : imageInfo.Info)
			outputFile.push_back(e);

         // 结束中断
		if (imageInfo.IsEnd)
			break;
	}
     // 未出错
	if (ret == 0)
	{
         // 等待线程结束（虽然感觉这里可能不需要）
		th.join();
		printf("\nVideo Parse is success.\nFile Size:%lldB\nTotal Frame:%d\n",outputFile.size(), precode);
         
         // 输出
		FILE*fp=fopen(filePath, "wb");
		if (fp == nullptr) return 1;
		outputFile.push_back('\0');
		fwrite(outputFile.data(),sizeof(unsigned char),outputFile.size()-1,fp);
		fclose(fp);
		return ret;
	}
    // 有错误，中断
	exit(1);
}
```

### ffmpeg.cpp

用于调用封装的ffmpeg指令

### code.cpp

用于将文件编成二维码形式

### ImgDecode.cpp

 用于将二维码解码成图片形式

### pic.cpp

用于对图像中的二维码定位
