//
//  tracked_object.cpp
//  
//
//  Created by Shari Matzner on 6/18/2015.
//  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
//

#include "tracked_object.h"

using namespace cv;
using namespace std;

/*
Track2D::Track2D(const Point2f& initial_pos,  long initial_epoch)
{
	position_.push_back(initial_pos);
	epoch_.push_back(initial_epoch);
}

void Track2D::update(const Point2f& newpos,  long epoch)
{
	position_.push_back(newpos);
	epoch_.push_back(epoch);
}
*/

TrackedObject::TrackedObject(const Point2f& initial_pos, InputArray initial_image, int initial_epoch, float process_noise, float measurement_noise)
{
    Mat initial_image_ = initial_image.getMat();
    CV_Assert( initial_image_.type() == CV_32FC1 || initial_image_.type() == CV_8UC1 || initial_image_.type() == CV_8UC3 || initial_image_.empty() );

	epoch_.push_back(initial_epoch);
	position_.push_back(initial_pos);
	image_.push_back(initial_image_);

    int Nstate = 4; // x, vx, y, vy
    int Nmeasure = 2; // x, y
	float q = process_noise; 
	float r = measurement_noise;
    
    
    kf_.init(Nstate, Nmeasure);
    // for extended Kalman filter
    //kf_.transitionMatrix = *(Mat_<float>(Nstate, Nstate) << 1, 1, 0, 1);
    Mat F = Mat::eye(Nstate, Nstate, CV_32FC1);
    F.at<float>(0,1) = 1;
    F.at<float>(2,3) = 1;
    F.copyTo(kf_.transitionMatrix);
	
    //cout << "kf_.transitionMatrix is " << kf_.transitionMatrix.rows << " x " << kf_.transitionMatrix.cols << endl;
	//cout << kf_.transitionMatrix << endl;
	
	kf_.measurementMatrix.at<float>(0,0) = 1.0;
	kf_.measurementMatrix.at<float>(1,2) = 1.0;

    //cout << "kf_.measurementMatrix is " << kf_.measurementMatrix.rows << " x " << kf_.measurementMatrix.cols << endl;
	//cout << kf_.measurementMatrix << endl;
	
    setIdentity(kf_.processNoiseCov, Scalar::all(q)); // values from example
	kf_.processNoiseCov.at<float>(0,1) = q;
	kf_.processNoiseCov.at<float>(1,0) = q;
	kf_.processNoiseCov.at<float>(2,3) = q;
	kf_.processNoiseCov.at<float>(3,2) = q;
	
    //cout << "kf_.processNoiseCov is " << kf_.processNoiseCov.rows << " x " << kf_.processNoiseCov.cols << endl;
	//cout << kf_.processNoiseCov << endl;	
	
    setIdentity(kf_.measurementNoiseCov, Scalar::all(r));
	
    //cout << "kf_.measurementNoiseCov is " << kf_.measurementNoiseCov.rows << " x " << kf_.measurementNoiseCov.cols << endl;
	//cout << kf_.measurementNoiseCov << endl;
	
    setIdentity(kf_.errorCovPost, Scalar::all(1));
    //randn(kf_.statePost, Scalar::all(0), Scalar::all(0.1));
    kf_.statePost = Scalar::all(0);
	kf_.statePost.at<float>(0,0) = initial_pos.x;
	kf_.statePost.at<float>(2,0) = initial_pos.y;
	
}


void TrackedObject::update( long epoch, const cv::Point2f& new_pos, InputArray new_image)
{
    Mat new_image_ = new_image.getMat();
    CV_Assert( new_image_.type() == CV_32FC1 || new_image_.type() == CV_8UC1 || new_image_.type() == CV_8UC3 || new_image_.empty() );

	epoch_.push_back(epoch);
	position_.push_back(new_pos);
	image_.push_back(new_image_);

    
    Mat_<float> measurement(2,1);
	measurement(0) = new_pos.x;
	measurement(1) = new_pos.y;
    kf_.correct(measurement);
}


Point2f TrackedObject::predict( long epoch)
{
    Mat prediction = kf_.predict();
	// elapsed time
	float dt = epoch - epoch_.back();
    kf_.transitionMatrix.at<float>(0,1) = dt;
    kf_.transitionMatrix.at<float>(2,3) = dt;
    return Point2f(prediction.at<float>(0),prediction.at<float>(2));
}

void  TrackedObject::get_track(vector<long>& epoch, vector<Point2f>& position, std::vector<cv::Mat>* img) const
{
        epoch.assign(epoch_.begin(), epoch_.end());
        position.assign(position_.begin(), position_.end());
        if (img != NULL) img->assign(image_.begin(), image_.end());
}

void  TrackedObject::get_track(std::vector<long>& epoch, std::vector<Rect>& bounding_box) const
{
    epoch.assign(epoch_.begin(), epoch_.end());
    bounding_box.clear();
    int N = epoch_.size();
    for (int k=0; k<N; ++k)
    {
        Rect bb(position_[k].x, position_[k].y, image_[k].cols, image_[k].rows);
        bounding_box.push_back(bb);
    }
}


void  TrackedObject::get_track_smoothed(vector<long>& epoch, vector<Point2f>& position) const
{
	epoch.assign(epoch_.begin(), epoch_.end());
	int N = epoch_.size();
	//cout << endl << "smoothing track of length " << N << endl;
	if (N < 3)
	{
		position.assign(position_.begin(), position_.end());
		return;
	}
	position.assign(N,Point2f(0,0));
	int win_size = (N<7) ? 3 : 5;
	//cout << "win_size = " << win_size << endl;
	int half_win = (win_size-1)/2;
	//cout << "half_win = " << half_win << endl;
	float one_over_winsize = 1.f/win_size;
	// first elements with padding
	for (int k=0;k<half_win;++k)
	{
		position[k] = position_[k];
		/*
		position[k] = (half_win - k)*position_[0];
		cout << "k = " << k << ", position[k] = " << position[k].x << "," << position[k].y;
		for (int j=k;j<k+half_win+1;++j)
			position[k]+=position_[j];
		cout << "  " << position[k].x << "," << position[k].y;
		position[k] = position[k]*one_over_winsize;
		cout << "  " << position[k].x << "," << position[k].y << endl;
		 */
	}
	for (int k=half_win;k<N-half_win;++k)
	{
		for (int j=k-half_win;j<k+half_win+1;++j)
			position[k]+=position_[j];
		position[k] = position[k]*one_over_winsize;
	}
	//last elements with padding
	for (int k=N-half_win;k<N;++k)
	{
		position[k] = position_[k];
		/*
		for (int j=k-half_win;j<N;++j)
			position[k]+=position_[j];
		position[k] += (half_win - (N-k)+1)*position_.back();
		position[k] = position[k]*one_over_winsize;
		 */
	}
}

void   TrackedObject::get_last_image(OutputArray lastimg) const
{
	if (image_.empty()) return;
	//cout << "size of image track is " << image_.size() << endl;
	Mat backimg = image_.back();
	lastimg.create(backimg.rows,backimg.cols,backimg.type());
	Mat _lastimg = lastimg.getMat();
	//cout << "lastimg_ size is " << _lastimg.rows << " x " << _lastimg.cols << endl;
	backimg.copyTo(_lastimg);
}





