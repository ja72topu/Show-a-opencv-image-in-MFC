Cstring images;// from "add variable" by click picture control in MFC dialog. 
cv::Mat cvImg=imread("C:\\Users\\mango\\Desktop\\c++\\imageread\\x64\\Release\\3.bmp");
CvMatToWinControl(cvImg, &images); // don't forget this symbol "&".
                                   // then you can see the images in picture box
