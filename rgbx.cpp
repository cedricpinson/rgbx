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

        // in our case,
        const double kRGBMMaxRange = _range;
        const double kOneOverRGBMMaxRange = 1.0 / kRGBMMaxRange;

        // encode to RGBM, c = RGBA colors in 0..1 floats
        double r = rgb[0] * kOneOverRGBMMaxRange;
        double g = rgb[1] * kOneOverRGBMMaxRange;
        double b = rgb[2] * kOneOverRGBMMaxRange;

        double a = std::max( std::max(r, g), std::max(b, 1e-6) );
        a = ceil(a * 255.0) / 255.0;

        rgbm[3] = (uint8_t)floor(255.0*std::min(a, 1.0));
        rgbm[0] = (uint8_t)floor(255.0*std::min(r / a, 1.0));
        rgbm[1] = (uint8_t)floor(255.0*std::min(g / a, 1.0));
        rgbm[2] = (uint8_t)floor(255.0*std::min(b / a, 1.0));
    }

    void decode( uint8_t rgbm[4], float rgb[3] ) const {
        double f = 1.0/(255.0*255.0);
        double a = rgbm[3] * f * _range;
        rgb[0] = rgbm[0] * a;
        rgb[1] = rgbm[1] * a;
        rgb[2] = rgbm[2] * a;
    }
};



/* Converts a float RGB pixel into byte RGB with a common exponent.
 * Source :http://www.graphics.cornell.edu/~bjw/rgbe.html
 */
struct RGBE : Operator {

    const char* getName() const { return "rgbe"; }
    void encode( float rgb[3], uint8_t rgbe[4]) const {

        float v = rgb[0];
        if(rgb[1] > v)
            v = rgb[1];
        if(rgb[2] > v)
            v = rgb[2];

        if(v < 1e-32) {
            rgbe[0] = rgbe[1] = rgbe[2] = rgbe[3] = 0;
        } else {
            int e;
            v = frexp(v, &e) * 256.0 / v;
            rgbe[0] = (unsigned char)(rgb[0] * v);
            rgbe[1] = (unsigned char)(rgb[1] * v);
            rgbe[2] = (unsigned char)(rgb[2] * v);
            rgbe[3] = (unsigned char)(e + 128);
        }
    }

    void decode( uint8_t rgbe[4], float rgb[3] ) const {
        double a = rgbe[3];
        double f = pow(2.0, a - (128.0 + 8.0));
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
        std::cout << "use range " << range << " for rgbm " << std::endl;
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
        specOut.attribute("oiio:UnassociatedAlpha", true);
        ImageBuf dst(filenameOut, specOut);


        ImageSpec spec = src.spec();
        int width = spec.width,
            height = spec.height;

        ImageBuf::Iterator<float, float> iteratorSrc(src, 0, width, 0, height);
        ImageBuf::Iterator<uint8_t, uint8_t> iteratorDst(dst, 0, width, 0, height);

        float result[3];

        for (; iteratorDst.valid(); iteratorDst++, iteratorSrc++) {
            iteratorSrc.pos( iteratorDst.x(), iteratorDst.y(), iteratorDst.z());

            float* in = (float*)iteratorSrc.rawptr();
            uint8_t* out = (uint8_t*)iteratorDst.rawptr();

            (*_operator).encode(in, out );
            // (*_operator).decode(out, result );

            // std::cout << abs(result[0]-in[0]) << " " << abs(result[1]-in[1]) << " " << abs(result[2]-in[2]) << std::endl;

        }

        dst.save();

    }

    void decode( const std::string& filenameIn, const std::string& filenameOut) {

        std::cout << "decode " << filenameIn << " to " << filenameOut << " with method " << _operator->getName() << std::endl;

        ImageBuf src(filenameIn);
        src.read();
        ImageSpec specIn = src.spec();

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

        dst.save();
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

        error.save();
    }


};


int usage( char* command) {
    std::cout << "usage: " << command << " [-d] [-m method] [-r range] input output" << std::endl;
    std::cout << "encode/decode image into rgbm/e png" << std::endl;
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
