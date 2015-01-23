#include <sstream>
#include <unistd.h>
#include <limits>
#include <algorithm>
#include <cmath>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

OIIO_NAMESPACE_USING


struct Operator {
    virtual ~Operator() {}
    virtual void encode( float rgb[3], uint8_t rgbm[4]) const = 0;
    virtual void decode( uint8_t rgbm[4], float rgb[3]) const = 0;
    virtual const char* getName() const = 0;
};


struct RGBM : Operator {
    const double _range;

    RGBM( double range ) : _range( range ) {
    }

    const char* getName() const { return "rgbm"; }
    void encode( float rgb[3], uint8_t rgbm[4]) const {

        const double oneOverRange = 1.0 / _range;

        double r = rgb[0] * oneOverRange;
        double g = rgb[1] * oneOverRange;
        double b = rgb[2] * oneOverRange;

        double maxRGB = std::max( r , std::max( g, b ) );
        double a = ceil(maxRGB * 255.0) / 255.0;

        rgbm[0] = round(255.0*std::min(r / a, 1.0));
        rgbm[1] = round(255.0*std::min(g / a, 1.0));
        rgbm[2] = round(255.0*std::min(b / a, 1.0));
        rgbm[3] = round(255.0*std::min(a, 1.0));
    }

    void decode( uint8_t rgbm[4], float rgb[3] ) const {
        double a = rgbm[3] * _range / ( 255.0 * 255.0 );
        rgb[0] = rgbm[0] * a;
        rgb[1] = rgbm[1] * a;
        rgb[2] = rgbm[2] * a;
    }
};


//http://vemberaudio.se/graphics/RGBdiv8.pdf
struct RGBD : Operator {

    const char* getName() const { return "rgbd"; }

    void encode( float rgb[3], uint8_t rgbm[4]) const {

        double maxRGB = std::max( std::max( rgb[0], 1.0f) , std::max( rgb[1], rgb[2] ) );
        if ( maxRGB > 8.0 ) {
            //std::cout << maxRGB << std::endl;
        }
        double f      = 255.0/maxRGB;
        rgbm[0]       = rgb[0] * f;
        rgbm[1]       = rgb[1] * f;
        rgbm[2]       = rgb[2] * f;
        rgbm[3]       = f;
    }

    void decode( uint8_t rgbm[4], float rgb[3] ) const {
        double f = 1.0 / rgbm[3];
        rgb[0]   = rgbm[0] * f;
        rgb[1]   = rgbm[1] * f;
        rgb[2]   = rgbm[2] * f;
    }
};


//http://iwasbeingirony.blogspot.fr/2010/06/difference-between-rgbm-and-rgbd.html
struct RGBDRange : Operator {

    const double _range;

    RGBDRange( double range): _range( range ) {
    }

    const char* getName() const { return "rgbd2"; }

    void encode( float rgb[3], uint8_t rgbm[4]) const {

        double maxRGB = std::max( std::max( rgb[0], 1.0f ) , std::max( rgb[1], rgb[2] ) );

        double D       = std::max( _range / maxRGB, 1.0);
        D              = std::min( floor( D ) / 255.0, 1.0);
        double f       = D * (255.0/_range);
        rgbm[0]        = 255.0 * rgb[0] * f;
        rgbm[1]        = 255.0 * rgb[1] * f;
        rgbm[2]        = 255.0 * rgb[2] * f;
        rgbm[3]        = 255.0 * D;
        // return float4(rgb.rgb * (D * (255.0 / MaxRange)), D);
    }

    void decode( uint8_t rgbm[4], float rgb[3] ) const {
        //return rgbd.rgb * ((MaxRange / 255.0) / rgbd.a);
        double f = _range / 255.0 / (rgbm[3]/255.0);
        rgb[0]   = rgbm[0] * f / 255.0;
        rgb[1]   = rgbm[1] * f / 255.0;
        rgb[2]   = rgbm[2] * f / 255.0;
    }
};


/* Converts a float RGB pixel into byte RGB with a common exponent.
 * Source :http://www.graphics.cornell.edu/~bjw/rgbe.html
 */
struct RGBE : Operator {

    const char* getName() const { return "rgbe"; }
    void encode( float rgb[3], uint8_t rgbe[4]) const {

        double maxRGB = std::max( rgb[0], std::max( rgb[1], rgb[2] ) );

        if(maxRGB < 1e-32) {
            rgbe[0] = rgbe[1] = rgbe[2] = rgbe[3] = 0;
        } else {
            int e;
            double v = frexp(maxRGB, &e) * 256.0 / maxRGB;
            rgbe[0] = (unsigned char)(rgb[0] * v);
            rgbe[1] = (unsigned char)(rgb[1] * v);
            rgbe[2] = (unsigned char)(rgb[2] * v);
            rgbe[3] = (unsigned char)(e + 128);
        }
    }

    void decode( uint8_t rgbe[4], float rgb[3] ) const {
        double a = rgbe[3];
        double f = ldexp(1.0, a - (128.0 + 8.0));
        rgb[0] = rgbe[0] * f;
        rgb[1] = rgbe[1] * f;
        rgb[2] = rgbe[2] * f;
    }

};



struct Process {

    Operator* _operator;
    bool _generateError;

    Process() {
        _generateError = true;
    }

    void setRGBM( double range ) {
        _operator= new RGBM( range );
        std::cout << "use range " << range << " for " << _operator->getName() << std::endl;
    }

    void setRGBD2(  double range ) {
        _operator = new RGBDRange( range );
        std::cout << "use range " << range << " for " << _operator->getName() << std::endl;
    }
    void setRGBD() {
        _operator = new RGBD();
        //std::cout << "use range " << range << " for " << _operator->getName() << std::endl;
    }

    void setRGBE() {
        _operator= new RGBE();
    }

    void encode( const std::string& filenameIn, const std::string& filenameOut) {

        std::cout << "encode " << filenameIn << " to " << filenameOut << " with method " << _operator->getName() << std::endl;

        ImageBuf src(filenameIn);
        src.read();
        ImageSpec specIn = src.spec();

        ImageSpec specOut(specIn.width, specIn.height, 4, TypeDesc::UINT8);
        specOut.attribute("oiio:UnassociatedAlpha", 1);
        ImageBuf dst(filenameOut, specOut);


        ImageSpec spec = src.spec();
        int width = spec.width,
            height = spec.height;

        ImageBuf::Iterator<float, float> iteratorSrc(src, 0, width, 0, height);
        ImageBuf::Iterator<uint8_t, uint8_t> iteratorDst(dst, 0, width, 0, height);

        float result[3];
        float inTmp[3];
        float* in;
        float biggest = 0.0;
        for (; iteratorDst.valid(); iteratorDst++, iteratorSrc++) {
            iteratorSrc.pos( iteratorDst.x(), iteratorDst.y(), iteratorDst.z());

            float* inRaw = (float*)iteratorSrc.rawptr();
            uint8_t* out = (uint8_t*)iteratorDst.rawptr();

            // we assume to have at least 3 channel in inputs, but it could be greyscale
            if ( specIn.nchannels < 3 ) {
                inTmp[0] = inRaw[0];
                inTmp[1] = inRaw[0];
                inTmp[2] = inRaw[0];
                in = inTmp;
            } else {
                in = inRaw;
            }

            (*_operator).encode(in, out );
            (*_operator).decode(out, result );

            biggest = std::max(
                std::max( double(biggest), fabs(result[0]-in[0]) ),
                std::max( fabs(result[1]-in[1]), fabs(result[2]-in[2]) ) );

        }
        std::cout << "error max " << biggest << std::endl;
        dst.write(filenameOut);

    }

    void decode( const std::string& filenameIn, const std::string& filenameOut) {

        std::cout << "decode " << filenameIn << " to " << filenameOut << " with method " << _operator->getName() << std::endl;

        ImageInput* image = ImageInput::create(filenameIn);
        ImageSpec config = ImageSpec();
        ImageSpec specIn = ImageSpec();
        config.attribute("oiio:UnassociatedAlpha", 1);
        image->open(filenameIn, specIn, config );
        uint8_t* data = new uint8_t[specIn.width * specIn.height * specIn.nchannels ];
        image->read_image( TypeDesc::UINT8, data );
        image->close();

        ImageBuf src(specIn, data);


        int width = specIn.width,
            height = specIn.height;

        ImageSpec specOut(specIn.width, specIn.height, 3, TypeDesc::FLOAT );
        ImageBuf dst(filenameOut, specOut);

        ImageBuf::Iterator<uint8_t, uint8_t> iteratorSrc(src, 0, width, 0, height);
        ImageBuf::Iterator<float, float> iteratorDst(dst, 0, width, 0, height);

        for (; iteratorDst.valid(); iteratorDst++, iteratorSrc++) {
            iteratorSrc.pos( iteratorDst.x(), iteratorDst.y(), iteratorDst.z());

            uint8_t* in = (uint8_t*)iteratorSrc.rawptr();
            float* out = (float*)iteratorDst.rawptr();

            (*_operator).decode(in, out );
        }

        dst.write(filenameOut);
    }


    void error(ImageBuf& src, ImageBuf& encoded, const std::string& errorFilename  ) {

        ImageSpec spec = src.spec();
        int width = spec.width,
            height = spec.height;

        //ImageSpec specError(spec.width, spec.height, 3, TypeDesc::FLOAT);
        ImageBuf error( errorFilename, spec);

        ImageBuf::Iterator<float, float> iteratorSrc(src, 0, width, 0, height);
        ImageBuf::Iterator<uint8_t, uint8_t> iteratorDst(encoded, 0, width, 0, height);
        ImageBuf::Iterator<float, float> iteratorError(error, 0, width, 0, height);

        float decoded[3];
        for (; iteratorDst.valid(); iteratorDst++, iteratorSrc++, iteratorError++) {
            iteratorDst.pos( iteratorDst.x(), iteratorDst.y(), iteratorDst.z());

            float* in = (float*)iteratorSrc.rawptr();
            uint8_t* encoded = (uint8_t*)iteratorDst.rawptr();
            float* error = (float*)iteratorError.rawptr();

            (*_operator).decode(encoded, decoded );
            error[0] = fabs(in[0] - decoded[0] );
            error[1] = fabs(in[1] - decoded[1] );
            error[2] = fabs(in[2] - decoded[2] );
        }

        error.write(errorFilename);
    }


};


int usage( char* command) {
    std::cout << "usage: " << command << " [-d] [-m method] [-r range] input output" << std::endl;
    std::cout << "encode/decode image into rgbm/rgbd/rgbe png" << std::endl;
    return 1;
}

int main(int argc, char* argv[]) {

    std::string input;
    std::string output;
    std::string method = "rgbm";
    double range = 8.0;
    bool encode = true;
    int c;

    while ((c = getopt(argc, argv, "dm:r:")) != -1) {
        switch (c) {
        case 'm':
            method = optarg; break;
        case 'r':
            range = atof(optarg); break;
        case 'd':
            encode = false; break;
        default:
            return usage(argv[0]);
        }
    }

    if ( optind < argc-1 ) {

        input = std::string( argv[optind] );
        output = std::string( argv[optind+1] );

        Process process;
        if ( method == "rgbm" ) {
            process.setRGBM( range );
        } else if ( method == "rgbd2" ) {
            process.setRGBD2( range );
        } else if ( method == "rgbd" ) {
            process.setRGBD();
        } else {
            process.setRGBE();
        }

        if ( encode ) process.encode( input, output );
        else process.decode( input, output );

    } else {

        return usage(argv[0]);

    }

    return 0;
}
