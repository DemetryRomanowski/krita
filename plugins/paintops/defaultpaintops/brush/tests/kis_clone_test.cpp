/*
 *  Copyright (c) 2016 Eugene Ingerman
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/**
 * Inpaint using the PatchMatch Algorithm
 *
 * | PatchMatch : A Randomized Correspondence Algorithm for Structural Image Editing
 * | by Connelly Barnes and Eli Shechtman and Adam Finkelstein and Dan B Goldman
 * | ACM Transactions on Graphics (Proc. SIGGRAPH), vol.28, aug-2009
 *
 * Original author Xavier Philippeau
 * Code adopted from: David Chatting https://github.com/davidchatting/PatchMatch
 */

#include <boost/multi_array.hpp>
#include <random>

#include "kis_clone_test.h"
#include "kis_debug.h"
#include "kis_paint_device_debug_utils.h"
#include "kis_random_accessor_ng.h"

#include <QTest>

#include <QList>
#include <kis_transform_worker.h>
#include <kis_filter_strategy.h>
#include "KoColor.h"
#include "KoColorSpace.h"
#include "KoChannelInfo.h"
#include "KoMixColorsOp.h"

#include <KisPart.h>
#include <kis_group_layer.h>
#include <qimage_based_test.h>
#include <stroke_testing_utils.h>
#include <brushengine/kis_paint_information.h>
#include <kis_canvas_resource_provider.h>
#include <brushengine/kis_paintop_preset.h>
#include <brushengine/kis_paintop_settings.h>
#include <plugins/impex/psd/psd_saver.h>

#define isOdd(x) ((x) & 0x01)

const int MAX_DIST = 65535;

inline void alignRectBy2(qint32 &x, qint32 &y, qint32 &w, qint32 &h)
{
    x -= isOdd(x);
    y -= isOdd(y);
    w += isOdd(x);
    w += isOdd(w);
    h += isOdd(y);
    h += isOdd(h);
}

class MaskedImage : public KisShared
{
private:
    KisPaintDeviceSP imageDev;
    KisPaintDeviceSP maskDev;
    QRect imageSize;
    typedef boost::multi_array<bool, 2> Mask_type;
    Mask_type maskCache;

    typedef boost::multi_array<quint8, 3> Image_type;
    Image_type imageCache;

    void cacheImageSize( void ){
        imageSize = imageDev->exactBounds();
    }

    void cacheImage(void){
        quint32 nChannels = imageDev->channelCount();
        const KoColorSpace* cs = imageDev->colorSpace();
        imageCache.resize(boost::extents[imageSize.width()][imageSize.height()][nChannels]);

        KisSequentialConstIterator it(imageDev, imageSize);
        for (int y = 0; y < imageSize.height(); y++) {
            for (int x = 0; x < imageSize.width(); x++) {
                const quint8* pixel = it.rawDataConst();
                for (quint32 chan = 0; chan < nChannels; ++chan) {
                    quint8 v = cs->scaleToU8(pixel, (qint32)chan);
                    imageCache[x][y][chan]=v;
                }
                it.nextPixel();
            }
        }
    }

    void cacheMask(void){
        Q_ASSERT(!imageSize.isEmpty() && imageSize.isValid());

        maskCache.resize(boost::extents[imageSize.width()][imageSize.height()]);

        KisSequentialConstIterator it(maskDev, QRect(0, 0, imageSize.width(), imageSize.height()));

        for (int y = 0; y < imageSize.height(); y++) {
            for (int x = 0; x < imageSize.width(); x++) {
                const quint8* pixel = it.rawDataConst();
                maskCache[x][y] = (*pixel<128);
                it.nextPixel();
            }
        }
    }

    void cacheEverything(){
        cacheImageSize();
        cacheImage();
        cacheMask();
    }

public:
    KisPaintDeviceSP getImageDev( void ) const { return imageDev; }
    KisPaintDeviceSP getMaskDev( void ) const { return maskDev; }

    void clearMask(void){
        QRect sz = maskDev->exactBounds();
        quint8 val = 0;
        maskDev->fill(sz.x(), sz.y(), sz.width(), sz.height(), &val);
    }

    void clone(KisPaintDeviceSP _imageDev, KisPaintDeviceSP _maskDev){
        imageDev = new KisPaintDevice(*_imageDev);
        maskDev = new KisPaintDevice(*_maskDev);
        cacheEverything();
    }

    KisSharedPtr<MaskedImage> clone(){
        return new MaskedImage(imageDev, maskDev);
    }

    MaskedImage(KisPaintDeviceSP _imageDev, KisPaintDeviceSP _maskDev){
        clone(_imageDev, _maskDev);
    }

    MaskedImage( const MaskedImage& other ){
        clone(other.getImageDev(), other.getMaskDev());
    }

    void downsampleRow(KisHLineConstIteratorNG& imageIt0, KisHLineConstIteratorNG& imageIt1,
                       KisHLineConstIteratorNG& maskIt0, KisHLineConstIteratorNG& maskIt1,
                       KisHLineIteratorNG& dstImageIt, KisHLineIteratorNG& dstMaskIt )
    {
        bool ret = true;

        const KoColorSpace* cs = imageDev->colorSpace();
        //average 4 pixels
        const quint8* pixels[4];
        static const qint16 weights[4]={64,64,64,63}; //weights sum to 255 for averaging
        while( ret ){
            //handle mask?
            pixels[0]=imageIt0.oldRawData();
            imageIt0.nextPixel();
            pixels[1]=imageIt0.oldRawData();
            ret &= imageIt0.nextPixel();
            pixels[2]=imageIt1.oldRawData();
            imageIt1.nextPixel();
            pixels[3]=imageIt1.oldRawData();
            ret &= imageIt1.nextPixel();

            cs->mixColorsOp()->mixColors(pixels, weights, 4, dstImageIt.rawData());
            dstImageIt.nextPixel();

            pixels[0]=maskIt0.oldRawData();
            maskIt0.nextPixel();
            pixels[1]=maskIt0.oldRawData();
            ret &= maskIt0.nextPixel();
            pixels[2]=maskIt1.oldRawData();
            maskIt1.nextPixel();
            pixels[3]=maskIt1.oldRawData();
            ret &= maskIt1.nextPixel();

            maskDev->colorSpace()->mixColorsOp()->mixColors(pixels, weights, 4, dstMaskIt.rawData());
            dstMaskIt.nextPixel();
        }
    }

    void downsample2x( void ){
        qint32 srcX, srcY, srcWidth, srcHeight;
        QRect srcRect = imageDev->exactBounds();
        srcRect.getRect(&srcX, &srcY, &srcWidth, &srcHeight);

        alignRectBy2(srcX, srcY, srcWidth, srcHeight);

        // Nothing to do
        if ((srcWidth < 1) || (srcHeight < 1))
            return;

        qint32 dstX = srcX / 2;
        qint32 dstY = srcY / 2;
        qint32 dstWidth = srcWidth / 2;
        qint32 dstHeight = srcHeight / 2;

        KisPaintDeviceSP destImageDev = new KisPaintDevice(imageDev->colorSpace());
        KisPaintDeviceSP destMaskDev = new KisPaintDevice(maskDev->colorSpace());

        KisHLineConstIteratorSP imageIt0 = imageDev->createHLineConstIteratorNG(srcX, srcY, srcWidth);
        KisHLineConstIteratorSP imageIt1 = imageDev->createHLineConstIteratorNG(srcX, srcY + 1, srcWidth);
        KisHLineConstIteratorSP maskIt0 = maskDev->createHLineConstIteratorNG(srcX, srcY, srcWidth);
        KisHLineConstIteratorSP maskIt1 = maskDev->createHLineConstIteratorNG(srcX, srcY + 1, srcWidth);
        KisHLineIteratorSP dstImageIt = destImageDev->createHLineIteratorNG(dstX, dstY, dstWidth);
        KisHLineIteratorSP dstMaskIt = destMaskDev->createHLineIteratorNG(dstX, dstY, dstWidth);

        for (int row = 0; row < dstHeight; ++row) {
            downsampleRow(*imageIt0, *imageIt1, *maskIt0, *maskIt1,
                          *dstImageIt, *dstMaskIt);

            imageIt0->nextRow(); imageIt0->nextRow();
            imageIt1->nextRow(); imageIt1->nextRow();

            maskIt0->nextRow(); maskIt0->nextRow();
            maskIt1->nextRow(); maskIt1->nextRow();

            dstImageIt->nextRow();
            dstMaskIt->nextRow();
        }

        imageDev = destImageDev;
        maskDev = destMaskDev;;
        cacheEverything();
    }

    KisSharedPtr<MaskedImage> upscale(int xsize, int ysize){
        QRect sz = size();

        KisSharedPtr<MaskedImage> scaledImage = this->clone();

        KoDummyUpdater updater;
        KisTransformWorker worker(scaledImage->getImageDev(), xsize/sz.width(), ysize/sz.height(), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                  &updater, KisFilterStrategyRegistry::instance()->value("Bilinear"));
        worker.run();

        KisTransformWorker workerMask(scaledImage->getMaskDev(), xsize/sz.width(), ysize/sz.height(), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                  &updater, KisFilterStrategyRegistry::instance()->value("Bilinear"));
        workerMask.run();

        scaledImage->cacheEverything();
        return scaledImage;
    }

    QRect size(){
        return imageSize;
    }

    int countMasked(void){
        int count = std::count( maskCache.origin(), maskCache.origin()+maskCache.num_elements(), true);
        return count;
    }

    bool isMasked( int x, int y ){
        return maskCache[x][y];
    }

//    const quint8* getImagePixel(int x, int y) {
//        KisRandomConstAccessorSP it = imageDev->createRandomConstAccessorNG(x, y);
//        return it->oldRawData(); //is this Ok to do?
//    }

    inline quint8 getImagePixelU8(int x, int y, int chan){
        return imageCache[x][y][chan];
    }

    QVector<float> getImagePixels(int x, int y){
        KisRandomAccessorSP it = imageDev->createRandomAccessorNG(x, y);
        quint8* value = it->rawData();
        const KoColorSpace* cs = imageDev->colorSpace();
        QVector<float> channels(cs->channelCount());
        cs->normalisedChannelsValue( value, channels );
        return channels;
    }

    void setImagePixels(int x, int y, const QVector<float>& channels){
        KisRandomAccessorSP it = imageDev->createRandomAccessorNG(x, y);
        quint8* value = it->rawData();
        const KoColorSpace* cs = imageDev->colorSpace();
        Q_ASSERT(channels.size()==cs->channelCount());
        cs->fromNormalisedChannelsValue(value, channels);
    }

    long distance( int x, int y, const MaskedImage& other, int xo, int yo ){
        int dsq = 0;
        for(quint32 chan=0; chan<imageDev->channelCount(); chan++){
            int v = imageCache[x][y][chan]-other.imageCache[xo][yo][chan];
            dsq += v*v;
        }
        return dsq;
    }

};
typedef KisSharedPtr<MaskedImage> MaskedImageSP;

struct NNPixel{
    int x;
    int y;
    int distance;
};
typedef boost::multi_array<NNPixel, 2> NNArray_type;

class NearestNeighborField : public KisShared
{
private:
    template< typename T> T random_int( T range ){
        static std::random_device rd;
        static std::mt19937 gen(rd());

        std::uniform_int_distribution<T> dis(0, range-1);
        return dis(gen);
    }

    //compute intial value of the distance term
    void initialize(void){
        for(int y=0; y<imSize.height(); y++){
            for(int x=0; x<imSize.width(); x++){
                field[x][y].distance = distance(x, y, field[x][y].x, field[x][y].y);

                //if the distance is "infinity", try to find a better link
                int iter=0;
                int maxretry=20;
                while( field[x][y].distance == MAX_DIST && iter<maxretry ){
                    field[x][y].x = random_int(imSize.width());
                    field[x][y].y = random_int(imSize.height());
                    field[x][y].distance = distance(x, y, field[x][y].x, field[x][y].y);
                    iter++;
                }
            }
        }
    }

    void init_similarity_curve(void){
        float s_zero = 0.999;
        float t_halfmax = 0.10;

        float x  = (s_zero-0.5)*2;
        float invtanh = 0.5*std::log((1.+x)/(1.-x));
        float coef = invtanh/t_halfmax;

        similarity.resize(MAX_DIST+1);
        for(int i=0; i<similarity.size(); i++){
            float t = (float)i/similarity.size();
            similarity[i] = 0.5-0.5*std::tanh(coef*(t-t_halfmax));
        }
    }

private:
    int patchSize; //patch size
public:
    MaskedImageSP input, output;
    QRect imSize;
    NNArray_type field;
    std::vector<float> similarity;
    quint32 nColors;
    QList<KoChannelInfo *> channels;

    typedef boost::multi_array<int, 2> HistArray_type;
    HistArray_type histogram;

public:
    NearestNeighborField(const MaskedImageSP _input, MaskedImageSP _output, int _patchsize) : input(_input), output(_output), patchSize(_patchsize){
        imSize = input->size();
        field.resize(boost::extents[imSize.width()][imSize.height()]);
        init_similarity_curve();

        nColors = input->getImageDev()->colorSpace()->colorChannelCount(); //only color count, doesn't include alpha channels
        channels = input->getImageDev()->colorSpace()->channels();
        histogram.resize(boost::extents[nColors][256]);

    }

    void randomize(void){
        for(int y=0; y<imSize.height(); y++){
            for(int x=0; x<imSize.width(); x++){
                field[x][y].x = random_int(imSize.width());
                field[x][y].y = random_int(imSize.height());
                field[x][y].distance = MAX_DIST;
            }
        }
        initialize();
    }

    //initialize field from an existing (possibly smaller) nearest neighbor field
    void initialize( const NearestNeighborField& nnf ){
        int xscale = imSize.width() / nnf.imSize.width();
        int yscale = imSize.height() / nnf.imSize.height();

        for(int y=0; y<imSize.height(); y++){
            for(int x=0; x<imSize.width(); x++){
                int xlow = std::min( x/xscale, nnf.imSize.width()-1 );
                int ylow = std::min( y/yscale, nnf.imSize.height()-1 );

                field[x][y].x = nnf.field[xlow][ylow].x*xscale;
                field[x][y].y = nnf.field[xlow][ylow].y*yscale;
                field[x][y].distance = MAX_DIST;
            }
        }
        initialize();
    }

    //multi-pass NN-field minimization (see "PatchMatch" - page 4)
    void minimize( int pass ){
        int min_x = 0;
        int min_y = 0;
        int max_x = imSize.width()-1;
        int max_y = imSize.height()-1;

        for(int i=0; i<pass; i++){
            //scanline order
            for(int y=min_y; y<max_y; y++)
                for( int x=min_x; x<=max_x; x++)
                    if(field[x][y].distance>0)
                        minimizeLink(x,y,1);

            //reverse scanline order
            for(int y=max_y; y>=min_y; y--)
                for(int x=max_x; x>=min_x; x--)
                    if(field[x][y].distance)
                        minimizeLink(x,y,-1);
        }
    }

    void minimizeLink(int x, int y, int dir){
        int xp, yp, dp;

        //Propagation Left/Right
        if( x-dir>0 && x-dir<imSize.width()){
            xp = field[x-dir][y].x+dir;
            yp = field[x-dir][y].y;
            dp = distance(x, y, xp, yp);
            if(dp < field[x][y].distance){
                field[x][y].x = xp;
                field[x][y].y = yp;
                field[x][y].distance = dp;
            }
        }

        //Propagation Up/Down
        if( y-dir>0 && y-dir<imSize.height()){
            xp = field[x][y].x;
            yp = field[x][y].y+dir;
            dp = distance(x, y, xp, yp);
            if(dp < field[x][y].distance){
                field[x][y].x = xp;
                field[x][y].y = yp;
                field[x][y].distance = dp;
            }
        }

        //Random search
        int wi = output->size().width();
        int xpi = field[x][y].x;
        int ypi = field[x][y].y;
        while(wi>0){
            xp = xpi + random_int(2*wi)-wi;
            yp = ypi + random_int(2*wi)-wi;
            xp = std::max(0, std::min(output->size().width()-1, xp));
            yp = std::max(0, std::min(output->size().height()-1, yp));

            dp = distance(x, y, xp, yp);
            if(dp < field[x][y].distance){
                field[x][y].x = xp;
                field[x][y].y = yp;
                field[x][y].distance = dp;
            }
            wi /= 2;
        }
    }

    //compute distance between two patches
    int distance( int x, int y, int xp, int yp )
    {
        long distance = 0;
        long wsum = 0;
        long ssdmax = 10*255*255;

        //for each pixel in the source patch
        for(int dy = -patchSize; dy<=patchSize; dy++){
            for(int dx = -patchSize; dx<=patchSize; dx++){
                wsum += ssdmax;
                int xks = x+dx;
                int yks = y+dy;

                if(xks<0 || xks>=input->size().width()){
                    distance += ssdmax;
                    continue;
                }

                if(yks<0 || yks>=input->size().height()){
                    distance += ssdmax;
                    continue;
                }

                //cannot use masked pixels as a valid source of information
                if(input->isMasked(xks, yks)){
                    distance += ssdmax;
                    continue;
                }

                //corresponding pixel in target patch
                int xkt=xp+dx;
                int ykt=yp+dy;
                if(xkt<0 || xkt>=output->size().width() ){
                    distance += ssdmax;
                    continue;
                }
                if(ykt<0 || ykt>=output->size().height()){
                    distance += ssdmax;
                    continue;
                }

                //cannot use masked pixels as a valid source of information
                if(output->isMasked(xkt, ykt)){
                    distance += ssdmax;
                    continue;
                }

                //SSD distance between pixels
                long ssd = input->distance(xks, yks, *output, xkt, ykt);
                distance += ssd;

            }
        }
        return (int)(MAX_DIST*distance / wsum);
    }


};
typedef KisSharedPtr<NearestNeighborField> NearestNeighborFieldSP;

class Inpaint{
private:
    MaskedImageSP initial;
    NearestNeighborFieldSP nnf_TargetToSource;
    int radius;
    QList<MaskedImageSP> pyramid;

    MaskedImageSP ExpectationMaximization(int level);
    void EM_Step(MaskedImageSP source, MaskedImageSP target, NearestNeighborFieldSP nnf, bool upscaled);

public:
    Inpaint( KisPaintDeviceSP dev, KisPaintDeviceSP devMask, int _radius ){
        initial = new MaskedImage(dev, devMask);
        radius = _radius;
    }
    MaskedImageSP patch(void);
};

class TestClone : public TestUtil::QImageBasedTest
{
public:
    TestClone() : QImageBasedTest("clonetest"){}
    virtual ~TestClone() {}
    void test();
    void testPatchMatch();
private:
    MaskedImageSP patchImage(KisPaintDeviceSP, KisPaintDeviceSP, int radius);
};

MaskedImageSP Inpaint::patch()
{
    MaskedImageSP source = initial->clone();

    pyramid.append(initial);

    QRect size = source->size();
    while((size.width() > radius) && (size.height() > radius)){
        if(source->countMasked() == 0)
            break;
        source->downsample2x();
        pyramid.append(source->clone());
        KIS_DUMP_DEVICE_2(pyramid.last()->getImageDev(), pyramid.last()->getImageDev()->exactBounds(),"image","/home/eugening/Projects/Pyramid");
        KIS_DUMP_DEVICE_2(pyramid.last()->getMaskDev(), pyramid.last()->getMaskDev()->exactBounds(),"mask","/home/eugening/Projects/Pyramid");

        size = source->size();
    }
    int maxlevel = pyramid.size();

    // The initial target is the same as the smallest source.
    // We consider that this target contains no masked pixels
    MaskedImageSP target = source->clone();
    target->clearMask();

    //recursively building nearest neighbor field
    for(int level=maxlevel-1; level>=1; level--){
        source = pyramid.at(level);
        if(level == maxlevel-1){
            //random initial guess
            nnf_TargetToSource = new NearestNeighborField( target, source, radius);
            nnf_TargetToSource->randomize();
        }
        else{
            NearestNeighborFieldSP new_nnf = new NearestNeighborField(target, source, radius);
            new_nnf->initialize(*nnf_TargetToSource);
            nnf_TargetToSource = new_nnf;
        }

        //Build an upscaled target by EM-like algorithm
        target = ExpectationMaximization(level);
    }

    return target;

}

//EM-Like algorithm (see "PatchMatch" - page 6)
//Returns a float sized target image
MaskedImageSP Inpaint::ExpectationMaximization( int level ){
    int iterEM = std::min(2*level, 4);
    int iterNNF = std::min(5, level);

    MaskedImageSP source = nnf_TargetToSource->output;
    MaskedImageSP target = nnf_TargetToSource->input;
    MaskedImageSP newtarget = nullptr;

    //EM loop
    for(int emloop=1; emloop<=iterEM; emloop++){
        //set the new target as current target
        if( !newtarget.isNull() ){
            nnf_TargetToSource->input = newtarget;
            newtarget = nullptr;
        }

        //minimize the NNF
        nnf_TargetToSource->minimize(iterNNF);

        //Now we rebuild the target using best patches from source
        MaskedImageSP newsource = nullptr;
        bool upscaled = false;

        // Instead of upsizing the final target, we build the last target from the next level source image
        // So the final target is less blurry (see "Space-Time Video Completion" - page 5)
        if( level>=1 && (emloop==iterEM) ){
            newsource = pyramid.at(level-1);
            QRect sz = newsource->size();
            newtarget = target->upscale(sz.width(), sz.height());
            upscaled = true;
        } else{
            newsource = pyramid.at(level);
            newtarget = target->clone();
            upscaled = false;
        }
        //EM Step
        EM_Step(newsource, newtarget, nnf_TargetToSource, upscaled);
    }
    return newtarget;
}

void Inpaint::EM_Step(MaskedImageSP source, MaskedImageSP target, NearestNeighborFieldSP nnf, bool upscaled){
    NNArray_type& field = nnf->field;
    int R = radius;
    const KoColorSpace* cs = nnf->input->getImageDev()->colorSpace();

    const QRect& sz = nnf->input->size();

    if(upscaled)
        R *= 2;

    //for each pixel in the target image
    for(int y=0; y<target->size().height(); y++){
        for(int x=0; x<target->size().width(); x++){
            //zero init histogram
            std::fill(nnf->histogram.origin(), nnf->histogram.origin()+nnf->histogram.num_elements(), 0);
            float wsum = 0.;

            //Estimation step
            //for all target patches containing the pixel
            for(int dy=-R; dy<=R; dy++){
                for(int dx=-R; dx<=R; dx++){
                    //xpt,ypt = center pixel of the target patch
                    int xpt = x+dx;
                    int ypt = y+dy;

                    //get best corrsponding source patch from the NNF
                    int xst, yst;
                    float w;
                    if(!upscaled){
                        if(xpt < 0 || xpt >= sz.width()) continue;
                        if(ypt < 0 || ypt >= sz.height()) continue;
                        xst=field[xpt][ypt].x;
                        yst=field[xpt][ypt].y;
                        w = nnf->similarity[field[xpt][ypt].distance];
                    } else{
                        if(xpt < 0 || xpt >= 2*sz.width()) continue;
                        if(ypt < 0 || ypt >= 2*sz.height()) continue;
                        xst=2*field[xpt/2][ypt/2].x + (xpt%2);
                        yst=2*field[xpt/2][ypt/2].y + (ypt%2);
                        w = nnf->similarity[field[xpt/2][ypt/2].distance];
                    }

                    //get pixel corresponding to (x,y) in the source patch
                    int xs = xst-dx;
                    int ys = yst-dy;
                    if(xs < 0 || xs >= sz.width()) continue;
                    if(ys < 0 || ys >= sz.height()) continue;

                    //add contribution of the source pixel
                    if( source->isMasked(xs, ys) ) continue;

                    int colorChan = 0;
                    for (int chan = 0; chan < cs->channelCount(); chan++) {
                        if (nnf->channels.at(chan)->channelType() != KoChannelInfo::ALPHA) {
                            quint8 colorValue = source->getImagePixelU8(x,y,chan);
                            nnf->histogram[colorChan][colorValue]+=w;
                            colorChan++;
                        }
                    }
                    wsum += w;
                }
            }
            //no significant contribution : conserve the values from previous target
            if(wsum<1)
                continue;

            //Maximization step
            //average the contributions of significant pixels (near the median)
            float lowth = 0.4 * wsum; //low threshold in the CDF
            float highth = 0.6 * wsum; //high threshold in the CDF
            int colorChan = 0;
            QVector<float> channel_values = target->getImagePixels(x, y);
            for (int chan = 0; chan < nnf->channels.size(); chan++) {
                if (nnf->channels.at(chan)->channelType() != KoChannelInfo::ALPHA) {
                    float cdf = 0;
                    float contrib = 0;
                    float wcontrib = 0;

                    for(int i=0; i<256; i++){
                        cdf += nnf->histogram[colorChan][i];
                        if(cdf < lowth)
                            continue;
                        contrib += i*nnf->histogram[colorChan][i];
                        wcontrib += nnf->histogram[colorChan][i];
                        if( cdf > highth )
                            break;
                    }
                    channel_values[chan] = contrib / wcontrib;
                    colorChan++;
                }
                target->setImagePixels(x, y, channel_values);
            }
        }
    }
}

MaskedImageSP TestClone::patchImage(KisPaintDeviceSP dev, KisPaintDeviceSP devMask, int radius)
{

    Inpaint inpaint(dev, devMask, radius);
    return inpaint.patch();
}

void TestClone::testPatchMatch()
{
    QImage mainImage("/home/eugening/Projects/patch-inpainting/cow.png");    //TestUtil::fetchDataFileLazy("fill1_main.png"));
    QVERIFY(!mainImage.isNull());

    QImage maskImage("/home/eugening/Projects/patch-inpainting/cow-mask.png");    //TestUtil::fetchDataFileLazy("fill1_main.png"));
    QVERIFY(!maskImage.isNull());

    KisPaintDeviceSP mainDev = new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8());
    mainDev->convertFromQImage(mainImage, 0);
    QRect rect = mainDev->exactBounds();

    KisPaintDeviceSP maskDev = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8()); //colorSpace(GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), QString()));
    maskImage.invertPixels(QImage::InvertRgba);
    maskDev->convertFromQImage(maskImage, 0);

    QRect rectMask = maskDev->exactBounds();
    //QVERIFY(rect==rectMask);

    KIS_DUMP_DEVICE_2(mainDev,rect,"maindev","/home/eugening/Projects/img");
    KIS_DUMP_DEVICE_2(maskDev,rect,"maskdev","/home/eugening/Projects/img");

    MaskedImageSP output = patchImage(mainDev, maskDev, 2);
    KIS_DUMP_DEVICE_2(output->getImageDev(),output->size(),"output","/home/eugening/Projects/Out");
}


void TestClone::test(void)
{
    KisSurrogateUndoStore *undoStore = new KisSurrogateUndoStore();

    KisImageSP image = createImage(undoStore);
    KisDocument *doc = KisPart::instance()->createDocument();
    doc->setCurrentImage(image);

    image->initialRefreshGraph();

    KisLayerSP layer = new KisPaintLayer(image, "clone", OPACITY_OPAQUE_U8, image->colorSpace());
    image->addNode(layer,image->root());

    KisPaintDeviceSP dev = layer->paintDevice(); //chld->paintDevice(); //
    KisPainter painter(dev);

    QScopedPointer<KoCanvasResourceManager> manager(
        utils::createResourceManager(image, layer, "Basic_circle.kpp"));

    KisPaintOpPresetSP preset =
        manager->resource(KisCanvasResourceProvider::CurrentPaintOpPreset).value<KisPaintOpPresetSP>();

    KisResourcesSnapshotSP resources =
        new KisResourcesSnapshot(image,
                                 layer,
                                 image->postExecutionUndoAdapter(),
                                 manager.data());
    resources->setupPainter(&painter);


    painter.setPaintColor(KoColor(Qt::black, image->colorSpace()));
    painter.setFillStyle(KisPainter::FillStyle::FillStyleForegroundColor);

    KisDistanceInformation dist;

    for(int x=100; x<200; x+=5){
        KisPaintInformation pi(QPointF(x, x), 1.0);
        painter.paintAt(pi,&dist);
    }
    painter.device()->setDirty(painter.takeDirtyRegion());

    image->refreshGraph();
    doc->saveNativeFormat("/home/eugening/Projects/test.kra");

    delete doc;
}

void KisCloneOpTest::testClone()
{
    TestClone t;
    //t.test();
    t.testPatchMatch();
}

QTEST_MAIN(KisCloneOpTest)
