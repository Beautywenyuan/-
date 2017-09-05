#include <Eigen/Core>
#include <Eigen/LU>
#include<opencv2/opencv.hpp>  
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <opencv2/core/eigen.hpp>
#include <Eigen/Dense>
#include "Marker.h"
using namespace std;
using namespace cv;

vector<cv::Point3f> m_markerCorners3d;
vector<cv::Point2f> m_markerCorners2d;
Size markerSize(100,100);

Mat camMatrix;
Mat distCoeff;
float m_minContourLengthAllowed=30;

//�����ܳ�
float perimeter(const std::vector<cv::Point2f> &a)
{
	float sum = 0, dx, dy;

	for (size_t i = 0; i<a.size(); ++i)
	{
		size_t i2 = (i + 1) % a.size();

		dx = a[i].x - a[i2].x;
		dy = a[i].y - a[i2].y;

		sum += sqrt(dx*dx + dy*dy);
	}

	return sum;
}


void findMarkerCandidates(const std::vector<std::vector<cv::Point>>& contours, std::vector<Marker>& detectedMarkers)
{
	std::vector<cv::Point>  approxCurve;
	std::vector<Marker> possibleMarkers;
	// For each contour, analyze if it is a paralelepiped likely to be the marker
	for (size_t i = 0; i<contours.size(); ++i)
	{
		// ��϶��������
		cv::approxPolyDP(contours[i], approxCurve, double(contours[i].size())*0.05, true);                  //input arrary�㼯    approxCurve ��ϵĵ�   ����������Ϊ����  ���ĸ���ʾΪ�պ�

		// ��Ѱ�ı�������
		if (approxCurve.size() != 4)
			continue;

		// �ж��ǲ���͹�����
		if (!cv::isContourConvex(approxCurve))
			continue;

		//�ĸ���֮��������Сֵ�˳����
		float minDist = 1e10;
		for (int i = 0; i<4; ++i)
		{
			cv::Point vec = approxCurve[i] - approxCurve[(i + 1) % 4];
			float squaredDistance = vec.dot(vec);
			minDist = std::min(minDist, squaredDistance);//ȡ������Сֵ
		}

		// Ԥ���趨����С���ֵ
		if (minDist < m_minContourLengthAllowed)
			continue;

		//�˳���������������������м���     
		Marker m;
		for (int i = 0; i<4; ++i)
		{
			m.points.push_back(cv::Point2f(approxCurve[i].x, approxCurve[i].y));
		}
		//��ʱ��Ե�����������ӵ�һ����͵ڶ����㣬������������ڵ����������ұ�������ʱ���
		cv::Point v1 = m.points[1] - m.points[0];
		cv::Point v2 = m.points[2] - m.points[0];
		double o = (v1.x * v2.y) - (v1.y * v2.x);
		//��������Ӧ�ý����ڶ����͵��ĸ����ﵽЧ��
		if (o  < 0.0)
		{
			std::swap(m.points[1], m.points[3]);
		}
		possibleMarkers.push_back(m);
	}
	//ȥ���ǵ��Ϊ�ӽ�������
	//����Ϊpair������������ѡ���Ե��˳�����һ��
	std::vector< std::pair<int, int> > tooNearCandidates;
	for (size_t i = 0; i<possibleMarkers.size(); ++i)
	{
		const Marker& m1 = possibleMarkers[i];
		//����߳��ľ�ֵ
		for (size_t j = i + 1; j<possibleMarkers.size(); ++j)
		{
			const Marker& m2 = possibleMarkers[j];
			float distSquared = 0;
			for (int c = 0; c<4; ++c)
			{
				cv::Point v = m1.points[c] - m2.points[c];
				distSquared += v.dot(v);
			}
			distSquared /= 4;
			//�ı������߳�
			if (distSquared < 50)
			{
				tooNearCandidates.push_back(std::pair<int, int>(i, j));
			}
		}
	}

	//������͵ĵ�vertor��Ϊ��־λ
	//ȥ����Ӱ��Ҫ��� - -
	std::vector<bool> removalMask(possibleMarkers.size(), false);
	//
	for (size_t i = 0; i<tooNearCandidates.size(); ++i)
	{
		float p1 = perimeter(possibleMarkers[tooNearCandidates[i].first].points);
		float p2 = perimeter(possibleMarkers[tooNearCandidates[i].second].points);
		size_t removalIndex;
		if (p1 > p2)
			removalIndex = tooNearCandidates[i].second;
		else
			removalIndex = tooNearCandidates[i].first;

		//ѡ�����е�һ�����˳�
		removalMask[removalIndex] = true;
	}

	//���ؾ��������˳�����������������Marker
	detectedMarkers.clear();
	for (size_t i = 0; i<possibleMarkers.size(); ++i)
	{
		if (!removalMask[i])
			detectedMarkers.push_back(possibleMarkers[i]);
	}
}


void detectMarkers(const cv::Mat& grayscale, std::vector<Marker>& detectedMarkers)
{
	cv::Mat canonicalMarker;
	std::vector<Marker> goodMarkers;

	//////////////͸�ӱ任��Ѱgoodmarker////////////////////
	for (size_t i = 0; i<detectedMarkers.size(); ++i)
	{
		Marker& marker = detectedMarkers[i];
		//�õ�͸�ӱ任�ı任����ͨ���ĸ�����õ�����һ�������Ǳ���ڿռ��е����꣬�ڶ����������ĸ���������ꡣ
		cv::Mat M = cv::getPerspectiveTransform(marker.points, m_markerCorners2d);   //�任��ϵ����

		// Transform image to get a canonical marker image
		// ͸�ӱ任�ɷ���ͼ��
		cv::warpPerspective(grayscale, canonicalMarker, M, markerSize);           //�����ı任 ��canonicalmarker��ͼ
		threshold(canonicalMarker, canonicalMarker, 125, 255, THRESH_BINARY | THRESH_OTSU); //OTSU determins threshold automatically.

		//��ʾ�任�ɹ���ͼ��
		//imshow("Gray Image1",canonicalMarker);
		int nRotations;

		//cout<<"canonicalMarker"<<canonicalMarker.size()<<endl;
		// ��Ǳ���ʶ����Ҫ�����������ؼ���ļ���ά���������Ϣ��������>//
		int id = Marker::getMarkerId(canonicalMarker, nRotations);
		//�ж��Ƿ����Ԥ����ά����Ϣ
		if (id != -1)
		{
			marker.id = id;

			//sort the points so that they are always in the same order no matter the camera orientation
			std::rotate(marker.points.begin(), marker.points.begin() + 4 - nRotations, marker.points.end());
			goodMarkers.push_back(marker);
			//cout << goodMarkers.data<< endl;
		}
	}

	//ϸ���ǵ�
	if (goodMarkers.size() > 0)
	{
		std::vector<cv::Point2f> preciseCorners(4 * goodMarkers.size());
		for (size_t i = 0; i<goodMarkers.size(); ++i)
		{
			Marker& marker = goodMarkers[i];
			for (int c = 0; c<4; ++c)
			{
				preciseCorners[i * 4 + c] = marker.points[c];
				//�������
				//cout << preciseCorners[i * 4 + c] << endl;
			}
		}

		//ϸ���ǵ㺯��
		cv::cornerSubPix(grayscale, preciseCorners, cvSize(5, 5), cvSize(-1, -1), cvTermCriteria(CV_TERMCRIT_ITER, 30, 0.1));

		//copy back
		//��ϸ��λ�ø��Ƹ���ǽǵ�
		for (size_t i = 0; i<goodMarkers.size(); ++i)
		{
			Marker& marker = goodMarkers[i];

			for (int c = 0; c<4; ++c)
			{
				marker.points[c] = preciseCorners[i * 4 + c];
				//���������
				//cout << marker.points[c] << endl;
			}
		}
	}

	//��ֵ������һ����������
	detectedMarkers = goodMarkers;
	//cout<<"detectedMarkers.size()"<<detectedMarkers.size()<<endl;
}


void estimatePosition(std::vector<Marker>& detectedMarkers)
{

	//ʹ��forѭ������
	for (size_t i = 0; i<detectedMarkers.size(); ++i)
	{
		Marker& m = detectedMarkers[i];
		cv::Mat Rvec;
		cv::Mat_<float> Tvec;
		cv::Mat raux, taux;
		//
		cv::solvePnP(m_markerCorners3d, m.points, camMatrix, distCoeff, raux, taux, false, CV_P3P);

		raux.convertTo(Rvec, CV_32F);    //��ת����
		taux.convertTo(Tvec, CV_32F);   //ƽ������

		cv::Mat_<float> rotMat(3, 3);
		cv::Rodrigues(Rvec, rotMat);
		// Copy to transformation matrix

		("S dcode");
		// Since solvePnP finds camera location, w.r.t to marker pose, to get marker pose w.r.t to the camera we invert it.
		//std::cout << " Tvec ( X<-, Y ^, Z * �� ��" << Tvec.rows << "x" << Tvec.cols << std::endl;
		//std::cout << Tvec <<endl;		//ƽ�ƾ���
		//std::cout << " Rvec ( X<-, Y ^, Z * �� ��" << Rvec.rows << "x" << Rvec.cols << std::endl;
		//std::cout << rotMat << endl;      //��ת����
		//std::cout << camMatrix << endl;      //��ת����
		//std::cout << distCoeff << endl;      //��ת����

		float theta_z = atan2(rotMat[1][0], rotMat[0][0])*57.2958;
		float theta_y = atan2(-rotMat[2][0], sqrt(rotMat[2][0] * rotMat[2][0] + rotMat[2][2] * rotMat[2][2]))*57.2958;
		float theta_x = atan2(rotMat[2][1], rotMat[2][2])*57.2958;

		//void cv::cv2eigen(const Mat &rotMat, Eigen::Matrix< float, 1, Eigen::Dynamic > &R_n);

		Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic> R_n;
		Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic> T_n;
		cv2eigen(rotMat, R_n);
		cv2eigen(Tvec, T_n);
		Eigen::Vector3f P_oc;

		P_oc = -R_n.inverse()*T_n;


		std::cout << "��������" << P_oc << std::endl;
		//std::cout << "��ת����" << raux << std::endl;
		//std::cout << "��������" << m.points << std::endl;
		//std::cout << "�������������" << m_markerCorners3d << std::endl;

		//std::cout << "\nX ��" << theta_x << std::endl;
		//std::cout << "Y ��" << theta_y << std::endl;
		//std::cout << "Z ��" << theta_z << std::endl;

	}
}



//Ѱ������������
void Marker_Detection(Mat& img, vector<Marker>& detectedMarkers)
{
	Mat imgGray;
	Mat imgByAdptThr;
	vector<vector<Point>> contours;


	//��ͼ��תΪ�Ҷ�ͼ
	cvtColor(img, imgGray, CV_BGRA2GRAY);

	//��ֵ��
	threshold(imgGray, imgByAdptThr, 160, 255, THRESH_BINARY_INV);

	//������ͱ�����
	morphologyEx(imgByAdptThr, imgByAdptThr, MORPH_OPEN, Mat());
	morphologyEx(imgByAdptThr, imgByAdptThr, MORPH_CLOSE, Mat());

	//�������
	std::vector<std::vector<cv::Point> > allContours;
	cv::findContours(imgByAdptThr, allContours, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE);
	contours.clear();
	for (size_t i = 0; i<allContours.size(); ++i)
	{
		int contourSize = allContours[i].size();
		if (contourSize > 4)
		{
			contours.push_back(allContours[i]);
		}
	}
	//�жϷ�0
	if (contours.size())
	{
		//Ѱ�ҷ�������������
		findMarkerCandidates(contours, detectedMarkers);
	}

	//�жϷ�0
	if (detectedMarkers.size())
	{
		//����Ƕ�ά����Ϣ
		detectMarkers(imgGray, detectedMarkers);//�Ҷ�ͼ����Ѱ��Ϣ

		//��������
		estimatePosition(detectedMarkers);
	}
}




int main()
{
	m_markerCorners3d.push_back(cv::Point3f(-70.0f, -70.0f, 0));
	m_markerCorners3d.push_back(cv::Point3f(+70.0f, -70.0f, 0));    //���Ͻ�Ϊԭ��
	m_markerCorners3d.push_back(cv::Point3f(+70.0f, +70.0f, 0));
	m_markerCorners3d.push_back(cv::Point3f(-70.0f, +70.0f, 0));

	m_markerCorners2d.push_back(cv::Point2f(0, 0));
	m_markerCorners2d.push_back(cv::Point2f(markerSize.width - 1, 0));
	m_markerCorners2d.push_back(cv::Point2f(markerSize.width - 1, markerSize.height - 1));
	m_markerCorners2d.push_back(cv::Point2f(0, markerSize.height - 1));

	camMatrix = (Mat_<double>(3, 3) << 598.29493, 0, 304.76898, 0, 597.56086, 233.34673, 0, 0, 1);
	distCoeff = (Mat_<double>(5, 1) << -0.53572,1.35993,-0.00244,0.00620,0.00000);

	int color_width = 1920; //color
	int color_height = 1080;

	VideoCapture capture(1);
	Mat frame;
	uchar *ppbuffer = frame.ptr<uchar>(0);

	while(1)
	{
		capture >> frame;

		Mat colorImg(color_height, color_width, CV_8UC4, (void*)ppbuffer);
		//���Ƹ�ȫ�ֱ���
		//colorImg.copyTo(colorsrc);      //??????????????????    
		//��ͨ�����ز�ɫͼ
		//Mat colorImg = cv::Mat::zeros(color_height, color_width, CV_8UC1);//the color image 
		//colorImg = ConvertMat(pBuffer_color, color_width, color_height);

		//������ʾ

		
			//ʹ�ö�ά�����
		vector<Marker> detectedM;
		Marker_Detection(frame, detectedM);
		for (int marknum = 0; marknum < detectedM.size(); ++marknum)
		{
			vector<Point3f> Coord;
			int validnum = 0;
			for (int c = 0; c < 4; ++c)
			{
				//����ǵ�����
				//cout << "(x, y)    =\t" << detectedMarkers[marknum].points[c] << endl;
				Point tempPoint = detectedM[marknum].points[c];

				//��ǽǵ�
				circle(frame, detectedM[marknum].points[c], 5, Scalar(0, 0, 255), -1, 2);
				line(frame, detectedM[marknum].points[(c + 1) % 4], detectedM[marknum].points[c], Scalar(0, 255, 0), 1, 8);
			}
		}

		
		//β����

		cv::imshow("suoxiao", frame);

		waitKey(1);

	}

}
