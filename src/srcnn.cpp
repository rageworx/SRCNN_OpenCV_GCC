/*******************************************************************************
 * SRCNN: Super-Resolution with deep Convolutional Neural Networks
 * ----------------------------------------------------------------------------
 * Current Author : Raphael Kim ( rageworx@gmail.com )
 * Latest update  : 2018-08-10
 * Pre-Author     : Wang Shu
 * Origin-Date    @ Sun 13 Sep, 2015
 * Descriptin ..
 *                 This source code modified version from Origianl code of Wang
 *                Shu's. All license following from origin.
 * Update,
 *    2019-08-23:
 *                 A confidence russian guy advanced two convolutions in once,
 *                See Convolution99x11().
*******************************************************************************/
#ifndef EXPORTLIBSRCNN

////////////////////////////////////////////////////////////////////////////////
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <unistd.h>
#include <pthread.h>
#ifndef NO_OMP
    #include <omp.h>
#endif

#include "srcnn.h"
#include "tick.h"

/* pre-calculated convolutional data */
#include "convdata.h"

////////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace cv;

////////////////////////////////////////////////////////////////////////////////

static float    image_multiply  = 2.0f;
static unsigned image_width     = 0;
static unsigned image_height    = 0;
static float    cnn_v           = 0.5f;
static bool     opt_verbose     = true;
static bool     opt_debug       = false;
static bool     opt_help        = false;
static int      t_exit_code     = 0;

static string   path_me;
static string   file_me;
static string   file_src;
static string   file_dst;

////////////////////////////////////////////////////////////////////////////////

#define DEF_STR_VERSION     "0.1.5.21"

////////////////////////////////////////////////////////////////////////////////

/* Function Declaration */
void Convolution99( Mat& src, Mat& dst, \
                    const float kernel[9][9], float bias);

void Convolution11( vector<Mat>& src, Mat& dst, \
                    const float kernel[CONV1_FILTERS], float bias);
                    
void Convolution99x11( Mat& src, vector<Mat>& dst, \
                       const ConvKernel64_99 kernel99, 
                       const ConvKernel1 bias99, \
                       const CvnvKernel21 kernel11, 
                       const ConvKernel2 bias11 );

void Convolution55( vector<Mat>& src, Mat& dst, \
                    const float kernel[32][5][5], float bias);
                    

////////////////////////////////////////////////////////////////////////////////

static int IntTrim(int a, int b, int c)
{
    int buff[3] = {a, c, b};
    return buff[ (int)(c > a) + (int)(c > b) ];
}

/***
 * FuncName : Convolution99
 * Function : Complete one cell in the first Convolutional Layer
 * Parameter    : src - the original input image
 *                dst - the output image
 *                kernel - the convolutional kernel
 *                bias - the cell bias
 * Output   : <void>
***/
void Convolution99(Mat& src, Mat& dst, const float kernel[9][9], float bias)
{
    int width, height, row, col, i, j;
    float temp;
    height = dst.rows;
    width = dst.cols;
    int rowf[height + 8], colf[width + 8];

    /* Expand the src image */
    for (row = 0; row < height + 8; row++)
    {
        rowf[row] = IntTrim(0, height - 1, row - 4);
    }
    for (col = 0; col < width + 8; col++)
    {
        colf[col] = IntTrim(0, width - 1, col - 4);
    }

    /* Complete the Convolution Step */
    for (row = 0; row < height; row++)
    {
        for (col = 0; col < width; col++)
        {
            /* Convolution */
            temp = 0;

            for (i = 0; i < 9; i++)
            {
                for (j = 0; j < 9; j++)
                {
                    temp += kernel[i][j] * src.at<unsigned char>(rowf[row + i], colf[col + j]);
                }
            }

            temp += bias;

            /* Threshold */
            temp = (temp < 0) ? 0 : temp;

            dst.at<float>(row, col) = temp;
        }
    }
}

/***
 * FuncName : Convolution11
 * Function : Complete one cell in the second Convolutional Layer
 * Parameter    : src - the first layer data
 *                dst - the output data
 *                kernel - the convolutional kernel
 *                bias - the cell bias
 * Output   : <void>
***/
void Convolution11(vector<Mat>& src, Mat& dst, const float kernel[CONV1_FILTERS], float bias)
{
    int width, height, row, col, i, j;
    float temp;
    height = dst.rows;
    width = dst.cols;

    for (row = 0; row < height; row++)
    {
        for (col = 0; col < width; col++)
        {
            /* Process with each pixel */
            temp = 0;

            for (i = 0; i < CONV1_FILTERS; i++)
            {
                temp += src[i].at<float>(row, col) * kernel[i];
            }
            temp += bias;

            /* Threshold */
            temp = (temp < 0) ? 0 : temp;

            dst.at<float>(row, col) = temp;
        }
    }
}

/***
 * FuncName : Convolution99x11
 * Function : Complete cells in first and second Convolutional Layers.
 * Parameter    : src - the first layer data
 *                dst - the output data
 *                kernel99, bias99, kernel11 and 
 *                bias11 matrix - each reference precalculated tables
 * Output   : <void>
***/
void Convolution99x11( Mat& src, vector<Mat>& dst,
                       const ConvKernel64_99 kernel99, const ConvKernel1 bias99,
                       const CvnvKernel21 kernel11, const ConvKernel2 bias11 )
{
    int width, height, row, col, i, j, k;
    float temp[CONV1_FILTERS];
    float result;
    height = src.rows;
    width = src.cols;
    int rowf[height + 8], colf[width + 8];

    /* Expand the src image */
    for (row = 0; row < height + 8; row++)
    {
        rowf[row] = IntTrim(0, height - 1, row - 4);
    }
    for (col = 0; col < width + 8; col++)
    {
        colf[col] = IntTrim(0, width - 1, col - 4);
    }

    /* Complete the Convolution Step */
    for (row = 0; row < height; row++)
    {
        for (col = 0; col < width; col++)
        {
            for (k = 0; k < CONV1_FILTERS; k++)
            {
                /* Convolution */
                temp[k] = 0.0;

                for (i = 0; i < 9; i++)
                {
                    for (j = 0; j < 9; j++)
                    {
                        temp[k] += kernel99[k][i][j] 
                                   * src.at<unsigned char>(rowf[row + i], colf[col + j]);
                    }
                }

                temp[k] += bias99[k];

                /* Threshold */
                temp[k] = (temp[k] < 0) ? 0 : temp[k];
            }

            /* Process with each pixel */
            for (k = 0; k < CONV2_FILTERS; k++)
            {
                result = 0.0;

                for (i = 0; i < CONV1_FILTERS; i++)
                {
                    result += temp[i] * kernel11[k][i];
                }
                result += bias11[k];

                /* Threshold */
                result = (result < 0) ? 0 : result;

                dst[k].at<float>(row, col) = result;
            }
        }
    }   
}

/***
 * FuncName : Convolution55
 * Function : Complete the cell in the third Convolutional Layer
 * Parameter : src - the second layer data
 *             dst - the output image
 *             kernel - the convolutional kernel
 *             bias - the cell bias
 * Output   : <void>
***/
void Convolution55(vector<Mat>& src, Mat& dst, const float kernel[32][5][5], float bias)
{
    int width, height, row, col, i, m, n;
    unsigned cnt;
    float temp;
    double temppixel;
    height = dst.rows;
    width = dst.cols;
    int rowf[height + 4], colf[width + 4];

    /* Expand the src image */
    for (row = 0; row < height + 4; row++)
    {
        rowf[row] = IntTrim(0, height - 1, row - 2);
    }
    for (col = 0; col < width + 4; col++)
    {
        colf[col] = IntTrim(0, width - 1, col - 2);
    }

    /* Complete the Convolution Step */
    for (row = 0; row < height; row++)
    {
        for (col = 0; col < width; col++)
        {
            temp = 0;

            for (i = 0; i < CONV2_FILTERS; i++)
            {
                temppixel = 0;
                for (m = 0; m < 5; m++)
                {
                    for (n = 0; n < 5; n++)
                    {
                        temppixel += \
                        kernel[i][m][n] * src[i].at<float>(rowf[row + m], colf[col + n]);
                    }
                }

                temp += temppixel;
            }

            temp += bias;

            /* Threshold */
            temp = IntTrim(0, 255, temp);

            dst.at<unsigned char>(row, col) = (unsigned char)temp;
        }
    }

}

void ConvolutionA( Mat& src, Mat& dst, float part )
{
    int width, height, row, col;
    float cnn, cub;
    height = src.rows;
    width = src.cols;

    #pragma omp parallel for
    for (row = 0; row < height; row++)
    {
        for (col = 0; col < width; col++)
        {
            /* Process with each pixel */
            cnn = src.at<unsigned char>(row, col);
            cub = dst.at<unsigned char>(row, col);
            cnn *= part;
            cnn += (1.0 - part) * cub;

            /* Threshold */
            cnn = IntTrim(0, 255, (int)(cnn + 0.5));

            dst.at<unsigned char>(row, col) = (unsigned char)cnn;
        }
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool parseArgs( int argc, char** argv )
{
    for( int cnt=0; cnt<argc; cnt++ )
    {
        string strtmp = argv[ cnt ];
        size_t fpos   = string::npos;

        if ( cnt == 0 )
        {
            fpos = strtmp.find_last_of( "\\" );

            if ( fpos == string::npos )
            {
                fpos = strtmp.find_last_of( "/" );
            }

            if ( fpos != string::npos )
            {
                path_me = strtmp.substr( 0, fpos );
                file_me = strtmp.substr( fpos + 1 );
            }
            else
            {
                file_me = strtmp;
            }
        }
        else
        {
            if ( strtmp.find( "--scale=" ) == 0 )
            {
                string strval = strtmp.substr( 8 );
                if ( strval.size() > 0 )
                {
                    float tmpfv = atof( strval.c_str() );
                    if ( tmpfv > 0.f )
                    {
                        image_multiply = tmpfv;
                    }
                }
            }
            else
            if ( strtmp.find( "--cnn=" ) == 0 )
            {
                string strval = strtmp.substr( 8 );
                if ( strval.size() > 0 )
                {
                    float tmpfv = atof( strval.c_str() );
                    if ( ( tmpfv > 0.f ) && ( tmpfv <= 1.0f ) )
                    {
                        cnn_v = tmpfv;
                    }
                }
            }
            else
            if ( strtmp.find( "--noverbose" ) == 0 )
            {
                opt_verbose = false;
            }
            else
            if ( strtmp.find( "--help" ) == 0 )
            {
                opt_help = true;
            }
            else
            if ( file_src.size() == 0 )
            {
                file_src = strtmp;
            }
            else
            if ( file_dst.size() == 0 )
            {
                file_dst = strtmp;
            }
        }
    }

    if (!opt_help)
    {
        if ( ( file_src.size() > 0 ) && ( file_dst.size() == 0 ) )
        {
            string convname = file_src;
            string srcext;

            // changes name without file extention.
            size_t posdot = file_src.find_last_of( "." );
            if ( posdot != string::npos )
            {
                convname = file_src.substr( 0, posdot );
                srcext   = file_src.substr( posdot );
            }

            convname += "_resized";
            if ( srcext.size() > 0 )
            {
                convname += srcext;
            }

            file_dst = convname;
        }

        if ( ( file_src.size() > 0 ) && ( file_dst.size() > 0 ) )
        {
            return true;
        }
    }

    return false;
}

void printTitle()
{
    printf( "%s : Super-Resolution with deep Convolutional Neural Networks\n",
            file_me.c_str() );
    printf( "(C)2018 Raphael Kim, (C)2014 Wang Shu., version %s\n",
            DEF_STR_VERSION );
    printf( "Built with OpenCV version %s\n", CV_VERSION );
}

void printHelp()
{
    printf( "\n" );
    printf( "    usage : %s (options) [source file name] ([output file name])\n", 
            file_me.c_str() );
    printf( "\n" );
    printf( "    _options_:\n" );
    printf( "\n" );
    printf( "        --scale=( ratio: 0.1 to .. ) : scaling by ratio.\n" );
    printf( "        --cnn=( 0.1 to 1.0 )         : CNN\n" );
    printf( "        --noverbose                  : turns off all verbose\n" );
    printf( "        --help                       : this help\n" );
    printf( "\n" );
}

void* pthreadcall( void* p )
{
     if ( opt_verbose == true )
    {
        printTitle();
        printf( "\n" );
        printf( "- Scale multiply ratio : %.2f\n", image_multiply );
        fflush( stdout );
    }

    /* Read the original image */
    Mat pImgOrigin;

    pImgOrigin = imread( file_src.c_str() );

    if ( pImgOrigin.empty() == false )
    {
        if ( opt_verbose == true )
        {
            printf( "- Image load : %s\n", file_src.c_str() );
            fflush( stdout );
        }
    }
    else
    {
        if ( opt_verbose == true )
        {
            printf( "- load failure : %s\n", file_src.c_str() );
        }

        t_exit_code = -1;
        pthread_exit( &t_exit_code );
    }

    // Test image resize target ...
    Size testsz = pImgOrigin.size();
    if ( ( ( (float)testsz.width * image_multiply ) <= 0.f ) ||
         ( ( (float)testsz.height * image_multiply ) <= 0.f ) )
    {
        if ( opt_verbose == true )
        {
            printf( "- Image scale error : ratio too small.\n" );
        }

        t_exit_code = -1;
        pthread_exit( &t_exit_code );
    }

    // -------------------------------------------------------------

    if ( opt_verbose == true )
    {
        printf( "- Image converting to Y-Cr-Cb : " );
        fflush( stdout );
    }

    unsigned perf_tick0 = tick::getTickCount();

    /* Convert the image from BGR to YCrCb Space */
    Mat pImgYCrCb;
    cvtColor(pImgOrigin, pImgYCrCb, CV_BGR2YCrCb);

    if ( pImgYCrCb.empty() == false )
    {
        if ( opt_verbose == true )
        {
            printf( "Ok.\n" );
            fflush( stdout );
        }
    }
    else
    {
        if ( opt_verbose == true )
        {
            printf( "Failure.\n" );
        }

        t_exit_code = -2;
        pthread_exit( &t_exit_code );
    }

    // ------------------------------------------------------------

    if ( opt_verbose == true )
    {
        printf( "- Splitting channels : " );
        fflush( stdout );
    }

    /* Split the Y-Cr-Cb channel */
    vector<Mat> pImgYCrCbCh(3);
    split(pImgYCrCb, pImgYCrCbCh);

    if ( pImgYCrCb.empty() == false )
    {
        if ( opt_verbose == true )
        {
            printf( "Ok.\n" );
            fflush( stdout );
        }
    }
    else
    {
        if ( opt_verbose == true )
        {
            printf( "Failure.\n" );
            t_exit_code = -3;
            pthread_exit( &t_exit_code );
        }
    }

    // ------------------------------------------------------------

    if ( opt_verbose == true )
    {
        printf( "- Resizing splitted channels with bicublic interpolation : " );
    }

    /* Resize the Y-Cr-Cb Channel with Bicubic Interpolation */
    vector<Mat> pImg(3);

    #pragma omp parallel for
    for (int i = 0; i < 3; i++)
    {
        Size newsz = pImgYCrCbCh[i].size();
        newsz.width  *= image_multiply;
        newsz.height *= image_multiply;

        resize( pImgYCrCbCh[i],
                pImg[i],
                newsz,
                0,
                0,
                CV_INTER_CUBIC );
    }

    if ( opt_verbose == true )
    {
        printf( "Ok.\n" );
    }

    // -----------------------------------------------------------

    int cnt = 0;

    /*************** The First + Second Layers ***************/

    if ( opt_verbose == true )
    {
        printf( "- Processing convolutional layer I + II ... " );
        fflush( stdout );
    }

    vector<Mat> pImgConv2(CONV2_FILTERS);
        
    for ( unsigned cnt=0; cnt<CONV2_FILTERS; cnt++)
    {
        pImgConv2[cnt].create( pImg[0].size(), CV_32F );
    }

    Convolution99x11( pImg[0], pImgConv2, \
                      weights_conv1_data, biases_conv1, 
                      weights_conv2_data, biases_conv2    );

    if ( opt_verbose == true )
    {
        printf( "completed.\n" );
        fflush( stdout );
    }

#if 0
    /******************* The Second Layer *******************/

    if ( opt_verbose == true )
    {
        printf( "- Processing convolutional layer II ... " );
        fflush( stdout );
    }

    vector<Mat> pImgConv2(CONV2_FILTERS);
    #pragma omp parallel for
    for ( unsigned cnt=0; cnt<CONV2_FILTERS; cnt++ )
    {
        pImgConv2[cnt].create(pImg[0].size(), CV_32F);
        Convolution11( pImgConv1,
                       pImgConv2[cnt],
                       weights_conv2_data[cnt],
                       biases_conv2[cnt]);
    }

    if ( opt_verbose == true )
    {
        printf( "completed.\n" );
        fflush( stdout );
    }
#endif 

    /******************* The Third Layer *******************/

    if ( opt_verbose == true )
    {
        printf( "- Processing convolutional layer III ... " );
        fflush( stdout );
    }

    Mat pImgConv3;
    pImgConv3.create(pImg[0].size(), CV_8U);
    Convolution55(pImgConv2, pImgConv3, weights_conv3_data, biases_conv3);

    if ( opt_verbose == true )
    {
        printf( "completed.\n");
        printf( "- Merging images : " );
        fflush( stdout );
    }
    
    // a russian guy's CNN --
    float partCNN = (cnn_v < 0.0) ? 0 : ((cnn_v > 1.0) ? 1.0 : cnn_v);
    ConvolutionA( pImgConv3, pImg[0], partCNN );

    /* Merge the Y-Cr-Cb Channel into an image */
    Mat pImgYCrCbOut;
    pImg[0] = pImgConv3;
    merge(pImg, pImgYCrCbOut);

    if ( opt_verbose == true )
    {
        printf( "Ok.\n" );
        fflush( stdout );
    }

    // ---------------------------------------------------------

    if ( opt_verbose == true )
    {
        printf( "- Converting channel to BGR : " );
        fflush ( stdout );
    }

    /* Convert the image from YCrCb to BGR Space */
    Mat pImgBGROut;
    cvtColor(pImgYCrCbOut, pImgBGROut, CV_YCrCb2BGR);

    unsigned perf_tick1 = tick::getTickCount();

    if ( pImgBGROut.empty() == false )
    {
        if ( opt_verbose == true )
        {
            printf( "Ok.\n" );
            printf( "- Writing result to %s : ", file_dst.c_str() );
            fflush( stdout );
        }

        imwrite( file_dst.c_str() , pImgBGROut);

        if ( opt_verbose == true )
        {
            printf( "Ok.\n" );
        }
    }
    else
    {
        if ( opt_verbose == true )
        {
            printf( "Failure.\n" );
        }

        t_exit_code = -10;
        pthread_exit( &t_exit_code );
    }

    if ( opt_verbose == true )
    {
        printf( "- Performace : %u ms took.\n", perf_tick1 - perf_tick0 );
    }

    fflush( stdout );

    t_exit_code = 0;
    pthread_exit( NULL );
    return NULL;
}

/***
 * FuncName : main
 * Function : the entry of the program
 * Parameter    : argc - the number of the initial parameters
 *        argv - the entity of the initial parameters
 * Output   : int 0 for normal / int 1 for failed
***/
int main( int argc, char** argv )
{
    if ( parseArgs( argc, argv ) == false )
    {
        printTitle();
        printHelp();
        fflush( stdout );
        return 0;
    }

    pthread_t ptt;
    int       tid = 0;

    if ( pthread_create( &ptt, NULL, pthreadcall, &tid ) == 0 )
    {
        // Wait for thread ends ..
        pthread_join( ptt, NULL );
    }
    else
    {
        printf( "Error: pthread failure.\n" );
    }

    return t_exit_code;
}
#endif /// of EXPORTLIBSRCNN
