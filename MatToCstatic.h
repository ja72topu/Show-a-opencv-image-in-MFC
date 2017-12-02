#pragma once
/*
* PkMatToGDI - http://www.pklab.net/index.php?id=390
* Copyright 2015 PkLab.net
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*  1. Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
*  2. Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the documentation
*     and/or other materials provided with the distribution.
*  3. The name of the author may not be used to endorse or promote products
*     derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
* EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include "stdafx.h"
#include <afxwin.h>
#include <WinGDI.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>


///<summary>PkMatToGDI Class
/// This class provides a method to fast draw (fitting or stretching) an OpenCV Mat image directly into MFC Gui.
/// It's optimized to display a feed from video (cam o file) and it can be used to display a single cv::Mat.
/// 
/// It uses GDI to transfer the image from OpenCV memory to DC of a CStatic Control like a Picture or Static Text.
///
/// <b>ThreadSafe</b>You can use this class also in a worker threads only if
/// <li>two threads can't have DCs to the same window at same time</li>
/// <li>two threads can't try to manipulate the same DC at same time.</li>
///</summary>
///<remark>Only Grey,RGB, and RGBA images are supported (please note RGB and not BGR)</remark>
///<remark>Windows GDI requires DWORD alignment for rows and continuous memory block for the source bitmap.
///This means that the cv::Mat you will provide should be continuous (no ROI) and have columns %4.
///The class checks the requirement and creates a right temporary image in case is needed (loosing a bit of time).
///
///To improve general memory management is strongly suggested to use always images where cols %4 =0 but if
///given image is not continuous the class will create the right image again.
///
///This class isn't memory consuming because it uses an internal cv::Mat to recycle always it's possible,
///in special case when you are rendering a video where all frames have almost same size.
///</remark>
///<author>Copyright 2015 PkLab.net</author>
///<see>http://www.pklab.net/index.php?id=390</see>
class PkMatToGDI
{
public:
	///<summary>Standard constructor</summary>
	///<param name="ctrl">Set the CStatic controls where the cv::Mat will be drawn</param>
	///<param name="autofit">
	///<code>true</code>:the cv::Mat will be fitted on width or height based on destination rectangle
	///<code>false</code>:the cv::Mat will be stretched into destination rectangle
	///</param>
	///<remark>In case you don't provide a valid ctrl at construction time you can use SetDestinationControl(...) later</remark>
	PkMatToGDI(CWnd* ctrl = NULL, bool autofit = true)
	{
		Init(ctrl, autofit);
	}

	///<summary>Standard de-constructor</summary>
	~PkMatToGDI() {};

	///<summary>Set the CStatic controls where the cv::Mat will be drawn</summary>
	///<param name="ctrl">a valid CStatic object</param>
	///<param name="autofit">set the autofit feature on/off (see SetAutofit() ) </param>
	bool SetDestination(CWnd* ctrl, bool autofit)
	{
		return Init(ctrl, autofit);
	}

	///<summary>Set the CStatic controls where the cv::Mat will be drawn</summary>
	///<param name="ctrl">a valid CStatic object</param>
	///<remark>autofit feature will not change</remark>
	bool SetDestination(CWnd* ctrl)
	{
		return Init(ctrl, m_autofit);
	}


	///<summary>Set autofit features on/off.</summary>
	///<param name="autofit">
	///<code>true</code>:the cv::Mat will be fitted on width or height based on destination rectangle
	///<code>false</code>:the cv::Mat will be stretched into destination rectangle
	///</param>
	void SetAutofit(bool autofit)
	{
		m_autofit = autofit;
	}


	///<summary>Draw a cv::Mat using the DC of current CStatic control.</summary>
	bool PkMatToGDI::DrawImg(const cv::Mat &img)
	{

		if (m_WinCtrl == NULL || img.empty())
			return false;

		/*
		CDC* pDC = m_WinCtrl->GetDC();
		if (pDC == NULL)
		return;
		HDC hDC = pDC->GetSafeHdc();
		*/
		CClientDC hDC(m_WinCtrl);
		int bpp = 8 * img.elemSize();
		assert((bpp == 8 || bpp == 24 || bpp == 32));

		int img_w = img.cols;
		int img_h = img.rows;

		cv::Rect rr;
		if (m_autofit)
		{
			if (m_ctrlRectRatio > (1.0*img_w / img_h))
			{
				// fit height
				rr.height = m_ctrlRectCv.height;
				rr.width = (int)floor(1.0*img_w * m_ctrlRectCv.height / img_h);
				//align center
				rr.x = (int)floor((m_ctrlRectCv.width - rr.width) / 2.0);
				rr.y = m_ctrlRectCv.y;
			}
			else
			{
				// fit width
				rr.width = m_ctrlRectCv.width;
				rr.height = (int)floor(1.0*img_h * m_ctrlRectCv.width / img_w);
				//align middle
				rr.x = m_ctrlRectCv.x;
				rr.y = (int)floor((m_ctrlRectCv.height - rr.height) / 2);
			}
		}
		else
		{
			//stretch
			rr = m_ctrlRectCv;
		}


		// The image must be padded 4bytes and must be continuous
		//int stride = ((((img.cols * bpp) + 31) & ~31) >> 3);
		int padding = 0;
		// 32 bit image is always DWORD aligned because each pixel requires 4 bytes
		if (bpp == 32)
			padding = 0;
		else if ((img.cols % 4) > 0)
			padding = 4 - (img.cols % 4);
		else
			padding = 0;

		cv::Mat tempimg;
		if (padding > 0 || img.isContinuous() == false)
		{
			// Adding needed columns on the right (max 3 px)

			// we use internal image to reuse the memory. Avoid to alloc new memory at each call due to img size changes rarely
			cv::copyMakeBorder(img, m_internalImg, 0, 0, 0, padding, cv::BORDER_CONSTANT, 0);
			tempimg = m_internalImg;
			// ignore (do not shows) the just added border
			//img_w = tempimg.cols;
		}
		else
		{
			tempimg = img;
		}



		BITMAPINFO* bmi;
		BITMAPINFOHEADER* bmih;
		if (bpp == 8)
		{
			bmi = m_bmiGrey;
		}
		else
		{
			bmi = m_bmiColor;
		}

		bmih = &(bmi->bmiHeader);
		bmih->biHeight = -tempimg.rows;
		bmih->biWidth = tempimg.cols;
		bmih->biBitCount = bpp;

		//------------------
		// DRAW THE IMAGE 

		//if source and destination are same size
		if (tempimg.size() == m_ctrlRectCv.size())
		{
			// tranfer memory block
			// NOTE: the padding border will be shown here. Anyway it will be max 3px width
			int numLines = SetDIBitsToDevice(hDC,
				m_ctrlRectCv.x, m_ctrlRectCv.y, m_ctrlRectCv.width, m_ctrlRectCv.height,
				0, 0, 0, tempimg.rows, tempimg.data, bmi, DIB_RGB_COLORS);
			if (numLines == 0)
				return false;

			m_destRectCv = m_ctrlRectCv;
			// all done
			return true;
		}

		//if destination rect is smaller of previous we need to clear the background
		if (m_destRectCv.width <= 0)
		{
			m_destRectCv = rr;
		}
		else if (rr != m_destRectCv)
		{
			BackgroundClear();
			m_destRectCv = rr;
		}

		//if destination width less than source width
		else if (m_destRectCv.width < img_w)
		{
			SetStretchBltMode(hDC, HALFTONE);
		}
		else
		{
			SetStretchBltMode(hDC, COLORONCOLOR);
		}

		//copy and stretch the image
		int numLines = StretchDIBits(hDC,
			m_destRectCv.x, m_destRectCv.y, m_destRectCv.width, m_destRectCv.height,
			0, 0, img_w, img_h,
			tempimg.data, bmi, DIB_RGB_COLORS, SRCCOPY);
		if (numLines == 0)
			return false;

		//all done
		return true;
	}

private:

	///<summary>Repaint the rectangle using current brush</summary>
	void BackgroundClear()
	{
		CClientDC hDC(m_WinCtrl);
		//the rectangle is outlined by using the current pen and filled by using the current brush
		::Rectangle(hDC, m_ctrlRectWin.left, m_ctrlRectWin.top, m_ctrlRectWin.right, m_ctrlRectWin.bottom);

	}
	///<summary>Initialize members.</summary>
	///<return>false if fail</return>
	bool Init(CWnd* ctrl, bool autofit)
	{
		m_WinCtrl = ctrl;
		if (m_WinCtrl == NULL)
			return false;

		m_autofit = autofit;

		m_WinCtrl->GetClientRect(&m_ctrlRectWin);

		m_ctrlRectCv.x = m_ctrlRectWin.left;
		m_ctrlRectCv.y = m_ctrlRectWin.top;
		m_ctrlRectCv.width = m_ctrlRectWin.right - m_ctrlRectWin.left;
		m_ctrlRectCv.height = m_ctrlRectWin.bottom - m_ctrlRectWin.top;
		m_ctrlRectRatio = 1.0*m_ctrlRectCv.width / m_ctrlRectCv.height;
		m_destRectCv = m_ctrlRectCv;

		BITMAPINFOHEADER*	bmih;
		//standard colour bitmapinfo
		m_bmiColor = (BITMAPINFO*)_bmiColorBuffer;
		bmih = &(m_bmiColor->bmiHeader);
		memset(bmih, 0, sizeof(*bmih));
		bmih->biSize = sizeof(BITMAPINFOHEADER);
		bmih->biWidth = 0;
		bmih->biHeight = 0;
		bmih->biPlanes = 1;
		bmih->biBitCount = 0;
		bmih->biSizeImage = bmih->biWidth * bmih->biHeight * bmih->biBitCount / 8;
		bmih->biCompression = BI_RGB;

		//grey scale bitmapinfo
		m_bmiGrey = (BITMAPINFO*)_bmiGreyBuffer;
		bmih = &(m_bmiGrey->bmiHeader);
		memset(bmih, 0, sizeof(*bmih));
		bmih->biSize = sizeof(BITMAPINFOHEADER);
		bmih->biWidth = 0;
		bmih->biHeight = 0;
		bmih->biPlanes = 1;
		bmih->biBitCount = 8;
		bmih->biSizeImage = bmih->biWidth * bmih->biHeight * bmih->biBitCount / 8;
		bmih->biCompression = BI_RGB;

		//create a grey scale palette
		RGBQUAD* palette = m_bmiGrey->bmiColors;
		int i;
		for (i = 0; i < 256; i++)
		{
			palette[i].rgbBlue = palette[i].rgbGreen = palette[i].rgbRed = (BYTE)i;
			palette[i].rgbReserved = 0;
		}
		return true;
	}

private:
	//Display mode. True=autofit, false=stretch
	bool m_autofit;
	// image used internally to reduce memory allocation due to DWORD padding requirement
	cv::Mat m_internalImg;
	// the CStatic control where to show the image
	CWnd* m_WinCtrl;
	// Clientrect related to the m_WinCtrl
	RECT m_ctrlRectWin;
	// Utility: same as m_ctrlRectWin but using cv::Rect 
	cv::Rect m_ctrlRectCv;
	//Internal: ratio width/height for m_ctrlRectWin
	double m_ctrlRectRatio;
	//Internal:The image size into m_ctrlRectWin. This might smaller due to autofit
	cv::Rect m_destRectCv;

	//Bitmap header for standard color image
	BITMAPINFO* m_bmiColor;
	uchar _bmiColorBuffer[sizeof(BITMAPINFO)]; //extra space for grey color table

											   //Bitmap header for grey scale image
	BITMAPINFO* m_bmiGrey;
	uchar _bmiGreyBuffer[sizeof(BITMAPINFO) + 256 * 4]; //extra space for grey color table

};



//Single function used for the article on the web site


void CvMatToWinControl(const cv::Mat& img, CStatic* WinCtrl)
{
if (WinCtrl == NULL || img.empty())
return;

int bpp = 8 * img.elemSize();
assert((bpp == 8 || bpp == 24 || bpp == 32));

//Get DC of your win control
CClientDC hDC(WinCtrl);

// This is the rectangle where the control is defined
// and where the image will appear
RECT rr;
WinCtrl->GetClientRect(&rr);
//rr.top AND rr.left are always 0
int rectWidth = rr.right;
int rectHeight = rr.bottom;

///------------------------------------
/// DWORD ALIGNMENT AND CONTINOUS MEMORY
/// The image must be padded 4bytes and must be continuous

int border = 0;
//32 bit image is always DWORD aligned because each pixel requires 4 bytes
if (bpp < 32)
{
border = 4 - (img.cols % 4);
}

cv::Mat tmpImg;
if (border > 0 || img.isContinuous() == false)
{
// Adding needed columns on the right (max 3 px)
cv::copyMakeBorder(img, tmpImg, 0, 0, 0, border, cv::BORDER_CONSTANT, 0);
}
else
{
tmpImg = img;
}

///----------------------
/// PREPARE BITMAP HEADER
/// The header defines format and shape of the source bitmap in memory ... this will produce needed bmi

uchar buffer[sizeof(BITMAPINFO) + 256 * 4];
BITMAPINFO* bmi = (BITMAPINFO*)buffer;
BITMAPINFOHEADER* bmih = &(bmi->bmiHeader);
memset(bmih, 0, sizeof(*bmih));
bmih->biSize = sizeof(BITMAPINFOHEADER);
bmih->biWidth = tmpImg.cols;
bmih->biHeight = -tmpImg.rows;// DIB are bottom ->top
bmih->biPlanes = 1;
bmih->biBitCount = bpp;
bmih->biCompression = BI_RGB;

//Sets the palette if image is grey scale
if (bpp == 8)
{
RGBQUAD* palette = bmi->bmiColors;
for (int i = 0; i < 256; i++)
{
palette[i].rgbBlue = palette[i].rgbGreen = palette[i].rgbRed = (BYTE)i;
palette[i].rgbReserved = 0;
}
}

/// -----------
/// Draw to DC

if (tmpImg.cols == rectWidth  && tmpImg.rows == rectHeight)
{
// source and destination have same size
// transfer memory block
// NOTE: the padding border will be shown here. Anyway it will be max 3px width

SetDIBitsToDevice(hDC,
//destination rectangle
0, 0, rectWidth, rectHeight,
0, 0, 0, tmpImg.rows,
tmpImg.data, bmi, DIB_RGB_COLORS);
}
else
{
// Image is bigger or smaller than into destination rectangle
// we use stretch in full rect

// destination rectangle
int destx = 0, desty = 0;
int destw = rectWidth;
int desth = rectHeight;

// rectangle defined on source bitmap
// using imgWidth instead of tmpImg.cols will ignore the padding border
int imgx = 0, imgy = 0;
int imgWidth = tmpImg.cols - border;
int imgHeight = tmpImg.rows;

StretchDIBits(hDC,
destx, desty, destw, desth,
imgx, imgy, imgWidth, imgHeight,
tmpImg.data, bmi, DIB_RGB_COLORS, SRCCOPY);
}
} //end function

