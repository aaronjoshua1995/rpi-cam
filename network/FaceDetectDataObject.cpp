#include "FaceDetectDataObject.h"

FaceDetectDataObject::FaceDetectDataObject() {
    mCx = 0;
    mCy = 0;
    mFx = 0;
    mFy = 0;
    mImageDimensions[0] = 0;
    mImageDimensions[1] = 0;

}

void FaceDetectDataObject::setImageDimensions(int w, int h)
{
    mImageDimensions[0] = w;
    mImageDimensions[1] = h;
}

void FaceDetectDataObject::setFaceCentreMass(int x, int y) {
    mFx = x;
    mFy = y;
    mCx = (mImageDimensions[0]/2) - x;
    mCy = (mImageDimensions[1]/2) - y;
}

std::string FaceDetectDataObject::getJSONDataObject()
{
    std::string obj = "{\"ts\":%.3f,\"cx\":%.3f,\"cy\":%.3f,\"w\":%.3f,\"h\":%.3f}\n";

    return std::string();
}
