/***************************************************************************//**
**  \mainpage Mesh-based Monte Carlo (MMC) - a 3D photon simulator
**
**  \author Qianqian Fang <q.fang at neu.edu>
**  \copyright Qianqian Fang, 2010-2025
**
**  \section sref Reference:
**  \li \c (\b Fang2010) Qianqian Fang, <a href="http://www.opticsinfobase.org/abstract.cfm?uri=boe-1-1-165">
**          "Mesh-based Monte Carlo Method Using Fast Ray-Tracing
**          in Plucker Coordinates,"</a> Biomed. Opt. Express, 1(1) 165-175 (2010).
**  \li \c (\b Fang2012) Qianqian Fang and David R. Kaeli,
**           <a href="https://www.osapublishing.org/boe/abstract.cfm?uri=boe-3-12-3223">
**          "Accelerating mesh-based Monte Carlo method on modern CPU architectures,"</a>
**          Biomed. Opt. Express 3(12), 3223-3230 (2012)
**  \li \c (\b Yao2016) Ruoyang Yao, Xavier Intes, and Qianqian Fang,
**          <a href="https://www.osapublishing.org/boe/abstract.cfm?uri=boe-7-1-171">
**          "Generalized mesh-based Monte Carlo for wide-field illumination and detection
**           via mesh retessellation,"</a> Biomed. Optics Express, 7(1), 171-184 (2016)
**  \li \c (\b Fang2019) Qianqian Fang and Shijie Yan,
**          <a href="http://dx.doi.org/10.1117/1.JBO.24.11.115002">
**          "Graphics processing unit-accelerated mesh-based Monte Carlo photon transport
**           simulations,"</a> J. of Biomedical Optics, 24(11), 115002 (2019)
**  \li \c (\b Yuan2021) Yaoshen Yuan, Shijie Yan, and Qianqian Fang,
**          <a href="https://www.osapublishing.org/boe/fulltext.cfm?uri=boe-12-1-147">
**          "Light transport modeling in highly complex tissues using the implicit
**           mesh-based Monte Carlo algorithm,"</a> Biomed. Optics Express, 12(1) 147-161 (2021)
**
**  \section slicense License
**          GPL v3, see LICENSE.txt for details
*******************************************************************************/

/***************************************************************************//**
\file    mmc_utils.c

\brief   MC simulation settings and command line option processing unit
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#ifdef _POSIX_SOURCE
    #include <sys/ioctl.h>
#endif
#include "mmc_utils.h"
#include "mmc_const.h"
#include "mmc_bench.h"
#include "mmc_neurojson.h"
#include "zmat/zmatlib.h"
#include "ubj/ubj.h"

#if defined(_WIN32) && defined(USE_OS_TIMER) && !defined(MCX_CONTAINER)
    #include "mmc_tictoc.h"
#endif

#ifdef MCX_EMBED_CL
    #include "mmc_core.clh"
#endif
#include "nifti1.h"

/**
 * Macro to load JSON keys
 */

#define FIND_JSON_KEY(id,idfull,parent,fallback,val) \
    ((tmp=cJSON_GetObjectItem(parent,id))==0 ? \
     ((tmp=cJSON_GetObjectItem(root,idfull))==0 ? fallback : tmp->val) \
     : tmp->val)

/**
 * Macro to load JSON object
 */

#define FIND_JSON_OBJ(id,idfull,parent) \
    ((tmp=cJSON_GetObjectItem(parent,id))==0 ? \
     ((tmp=cJSON_GetObjectItem(root,idfull))==0 ? NULL : tmp) \
     : tmp)

#define UBJ_WRITE_KEY(ctx, key,  type, val)    {ubjw_write_key( (ctx), (key)); ubjw_write_##type((ctx), (val));}
#define UBJ_WRITE_ARRAY(ctx, type, nlen, val)  {ubjw_write_buffer( (ctx), (uint8_t*)(val), JDB_##type, (nlen));}

#define ubjw_write_single ubjw_write_float32
#define ubjw_write_double ubjw_write_float64
#define ubjw_write_uint16 ubjw_write_int16
#define ubjw_write_uint32 ubjw_write_int32
#define ubjw_write_uint64 ubjw_write_int64

/**
 * Macro to include unit name and line number in the error message
 */
#define MMC_ASSERT(id)   mcx_assert(id,__FILE__,__LINE__)


#define MIN_HEADER_SIZE 348    /**< Analyze header size */
#define NII_HEADER_SIZE 352    /**< NIFTI header size */
#define GL_RGBA32F 0x8814

/**
 * Short command line options
 * If a short command line option is '-' that means it only has long/verbose option.
 * Array terminates with '\0'.
 */

const char shortopt[] = {'h', 'E', 'f', 'n', 'A', 't', 'T', 's', 'a', 'g', 'b', 'D', 'G',
                         'd', 'r', 'S', 'e', 'U', 'R', 'l', 'L', 'I', '-', 'u', 'C', 'M',
                         'i', 'V', 'O', '-', 'F', 'q', 'x', 'P', 'k', 'v', 'm', '-', '-',
                         'J', 'o', 'H', '-', 'W', 'X', '-', 'c', 'Q', '-', 'Z', 'N', '\0'
                        };

/**
 * Long command line options
 * The length of this array must match the length of shortopt[], terminates with ""
 */

const char* fullopt[] = {"--help", "--seed", "--input", "--photon", "--autopilot",
                         "--thread", "--blocksize", "--session", "--array",
                         "--gategroup", "--reflect", "--debug", "--gpu", "--savedet",
                         "--repeat", "--save2pt", "--minenergy",
                         "--normalize", "--skipradius", "--log", "--listgpu",
                         "--printgpu", "--root", "--unitinmm", "--basisorder",
                         "--method", "--interactive", "--specular", "--outputtype",
                         "--momentum", "--outputformat", "--saveseed", "--saveexit",
                         "--replaydet", "--voidtime", "--version", "--mc", "--atomic",
                         "--debugphoton", "--compileropt", "--optlevel", "--maxdetphoton",
                         "--buffer", "--workload", "--saveref", "--gridsize", "--compute",
                         "--bench", "--dumpjson", "--zip", "--net", ""
                        };

extern char pathsep;

/**
 * Debug flags
 * R: debug random number generator
 * M: record photon movement and trajectory
 * P: show progress bar
 */

const char debugflag[] = {'S', 'C', 'B', 'W', 'D', 'I', 'O', 'X', 'A', 'T', 'R', 'P', 'E', 'M', '\0'};

/**
 * Selecting mesh-based ray-tracing algorithm:
 * p: Plucker-based ray-tracer, see Fang2010
 * h: Havel-based SSE4 ray-tracer, see Fang2012
 * b: Badouel ray-tracing algorithm, see Fang2011
 * s: branch-less Badouel SSE4 ray-tracer, see Fang2011
 * g: grid-output using dual-mesh MMC
 */

const char raytracing[] = {'p', 'h', 'b', 's', 'g', '\0'};

/**
 * Output data types
 * x: fluence rate
 * f: fluence
 * e: energy deposition
 * j: jacobian for mua
 * p: scattering counts for computing Jacobians for mus
 */

const char outputtype[] = {'x', 'f', 'e', 'j', 'l', 'p', '\0'};

/**
 * Output file format
 * mc2: binary mc2 format to store fluence volume data
 * nii: output fluence in nii format
 * hdr: output volume in Analyze hdr/img format
 * ubj: output volume in unversal binary json format (not implemented)
 */

const char* outputformat[] = {"ascii", "bin", "nii", "hdr", "mc2", "tx3", "jnii", "bnii", ""};

/**
 * Source type specifier
 * User can specify the source type using a string
 */

const char* srctypeid[] = {"pencil", "isotropic", "cone", "gaussian", "planar",
                           "pattern", "fourier", "arcsine", "disk", "fourierx", "fourierx2d", "zgaussian", "line", "slit", ""
                          };

/**
 * Flag to decide if parameter has been initialized over command line
 */

char flagset[256] = {'\0'};


/**
 * Flag for JData compression methods
 */

const char* zipformat[] = {"zlib", "gzip", "base64", "lzip", "lzma", "lz4", "lz4hc", ""};

/**
 * Flag to decide which platform to run mmc
 */

const char* computebackend[] = {"sse", "opencl", "cuda", ""};

/**
 * @brief Initializing the simulation configuration with default values
 *
 * Constructor of the simulation configuration, initializing all field to default values
 */

void mcx_initcfg(mcconfig* cfg) {
    cfg->medianum = 0;
    cfg->srcnum = 1;
    cfg->detnum = 0;
    cfg->e0 = 0;
    cfg->dim.x = 0;
    cfg->dim.y = 0;
    cfg->dim.z = 0;
    cfg->steps.x = 1.f;
    cfg->steps.y = 1.f;
    cfg->steps.z = 1.f;
    cfg->nblocksize = 64;
    cfg->nphoton = 0;
    cfg->nthread = 1024 * 8;
    cfg->seed = 0x623F9A9E;
    cfg->isrowmajor = 0;    /* not needed */
    cfg->maxgate = 1;
    cfg->implicit = 0;
    cfg->isreflect = 1;
    cfg->isref3 = 1;
    cfg->isnormalized = 1;
    cfg->issavedet = 0;
    cfg->respin = 1;
    cfg->issave2pt = 1;
    cfg->isgpuinfo = 0;
    cfg->basisorder = 1;
    cfg->compute = cbOpenCL;

    cfg->isdumpjson = 0;
    cfg->zipid = zmZlib;
    memset(cfg->jsonfile, 0, MAX_PATH_LENGTH);
    cfg->shapedata = NULL;

#if defined(USE_OPENCL) || defined(USE_CUDA)
    cfg->method = rtBLBadouelGrid;
#else
#ifndef MMC_USE_SSE
    cfg->method = rtPlucker;
#else
    cfg->method = rtHavel;
#endif
#endif
    cfg->prop = NULL;
    cfg->detpos = NULL;
    cfg->vol = NULL;
    cfg->session[0] = '\0';
    cfg->meshtag[0] = '\0';
    cfg->minenergy = 1e-6f;
    cfg->flog = stdout;
    cfg->sradius = 0.f;
    cfg->rootpath[0] = '\0';
    cfg->seedfile[0] = '\0';
    cfg->debuglevel = 0;
    cfg->minstep = 1.f;
    cfg->roulettesize = 10.f;
    cfg->nout = 1.f;
    cfg->unitinmm = 1.f;
    cfg->srctype = 0;
    cfg->isspecular = 0;
    cfg->issaveref = 0;
    cfg->outputtype = otFlux;
    cfg->outputformat = ofASCII;
    cfg->ismomentum = 0;
    cfg->issaveseed = 0;
    cfg->issaveexit = 0;
    cfg->photonseed = NULL;
    cfg->replaydet = 0;
    cfg->replayweight = NULL;
    cfg->replaytime = NULL;
    cfg->isextdet = 0;
    cfg->srcdir.w = 0.f;
    cfg->isatomic = 1;
    cfg->debugphoton = -1;
    cfg->savedetflag = 0x47;
    cfg->mediabyte = 1;

    cfg->tstart = 0.f;
    cfg->tstep = 0.f;
    cfg->tend = 0.f;

    cfg->mcmethod = mmMCX;

    memset(&(cfg->his), 0, sizeof(history));
    cfg->his.version = 1;
    cfg->his.unitinmm = 1.f;
    cfg->his.normalizer = 1.f;
    cfg->his.respin = 1;
    cfg->his.srcnum = cfg->srcnum;
    cfg->his.savedetflag = 0;

    memcpy(cfg->his.magic, "MCXH", 4);

    memset(&(cfg->srcpos), 0, sizeof(float3));
    memset(&(cfg->srcdir), 0, sizeof(float3));
    memset(&(cfg->bary0), 0, sizeof(float4));
    memset(&(cfg->srcparam1), 0, sizeof(float4));
    memset(&(cfg->srcparam2), 0, sizeof(float4));
    cfg->srcpattern = NULL;
    cfg->voidtime = 1;
    memset(cfg->checkpt, 0, sizeof(unsigned int)*MAX_CHECKPOINT);

    memset(&(cfg->detparam1), 0, sizeof(float4));
    memset(&(cfg->detparam2), 0, sizeof(float4));
    cfg->detpattern = NULL;

    cfg->optlevel = 3;

    memset(cfg->deviceid, 0, MAX_DEVICE);
    memset(cfg->workload, 0, MAX_DEVICE * sizeof(float));
    cfg->deviceid[0] = '1'; /*use the first GPU device by default*/
    memset(cfg->compileropt, 0, MAX_PATH_LENGTH);
    memset(cfg->kernelfile, 0, MAX_SESSION_LENGTH);
    cfg->maxdetphoton = 1000000;
    cfg->exportfield = NULL;
    cfg->exportdetected = NULL;
    cfg->exportseed = NULL;
    cfg->detectedcount = 0;
    cfg->energytot = NULL;
    cfg->energyesc = NULL;
    cfg->runtime = 0;
    cfg->autopilot = 1;
    cfg->gpuid = 0;
    cfg->maxjumpdebug = 10000000;
    cfg->exportdebugdata = NULL;
    cfg->debugdatalen = 0;
    cfg->nodenum = 0;
    cfg->elemnum = 0;
    cfg->elemlen = 0;
    cfg->node = NULL;
    cfg->elem = NULL;
    cfg->roitype = rtNone;
    cfg->roidata = NULL;

#ifdef MCX_EMBED_CL
    cfg->clsource = (char*)mmc_core_cl;
#else
    cfg->clsource = NULL;
#endif
#ifdef MCX_CONTAINER
    cfg->parentid = mpMATLAB;
#else
    cfg->parentid = mpStandalone;
#endif
}

/**
 * @brief Clearing the simulation configuration data structure
 *
 * Destructor of the simulation configuration, delete all dynamically allocated members
 */

void mcx_clearcfg(mcconfig* cfg) {
    if (cfg->medianum) {
        free(cfg->prop);
    }

    if (cfg->detnum) {
        free(cfg->detpos);
    }

    if (cfg->vol) {
        free(cfg->vol);
    }

    if (cfg->srcpattern) {
        free(cfg->srcpattern);
    }

    if (cfg->detpattern) {
        free(cfg->detpattern);
    }

    if (cfg->photonseed) {
        free(cfg->photonseed);
    }

    if (cfg->replayweight) {
        free(cfg->replayweight);
    }

    if (cfg->replaytime) {
        free(cfg->replaytime);
    }

    if (cfg->exportseed) {
        free(cfg->exportseed);
    }

    if (cfg->exportdetected) {
        free(cfg->exportdetected);
    }

    if (cfg->exportdebugdata) {
        free(cfg->exportdebugdata);
    }

    if (cfg->flog && cfg->flog != stdout && cfg->flog != stderr) {
        fclose(cfg->flog);
    }

    if (cfg->shapedata) {
        free(cfg->shapedata);
    }

    if (cfg->node) {
        free(cfg->node);
    }

    if (cfg->elem) {
        free(cfg->elem);
    }

    if (cfg->roidata) {
        free(cfg->roidata);
    }

    if (cfg->energytot) {
        free(cfg->energytot);
    }

    if (cfg->energyesc) {
        free(cfg->energyesc);
    }

#ifndef MCX_EMBED_CL

    if (cfg->clsource && cfg->clsource != (char*)mmc_core_cl) {
        free(cfg->clsource);
    }

#endif
    mcx_initcfg(cfg);
}

/**
 * @brief Reset and clear the GPU information data structure
 *
 * Clearing the GPU information data structure
 */

void mcx_cleargpuinfo(GPUInfo** gpuinfo) {
    if (*gpuinfo) {
        free(*gpuinfo);
        *gpuinfo = NULL;
    }
}


#ifndef MCX_CONTAINER

/**
 * @brief Save volumetric output (fluence etc) to an Nifty format binary file
 *
 * @param[in] dat: volumetric data to be saved
 * @param[in] len: total byte length of the data to be saved
 * @param[in] name: output file name (will append '.nii')
 * @param[in] type32bit: type of the data, only support 32bit per record
 * @param[in] outputformatid: decide if save as nii or analyze format
 * @param[in] cfg: simulation configuration
 */

void mcx_savenii(OutputType* dat, size_t len, char* name, int type32bit, int outputformatid, mcconfig* cfg) {
    FILE* fp;
    char fname[MAX_PATH_LENGTH] = {'\0'};
    nifti_1_header hdr;
    nifti1_extender pad = {{0, 0, 0, 0}};
    OutputType* logval = dat;
    size_t i;

    memset((void*)&hdr, 0, sizeof(hdr));
    hdr.sizeof_hdr = MIN_HEADER_SIZE;
    hdr.dim[0] = 4;
    hdr.dim[1] = cfg->dim.x;
    hdr.dim[2] = cfg->dim.y;
    hdr.dim[3] = cfg->dim.z;
    hdr.dim[4] = len / (cfg->dim.x * cfg->dim.y * cfg->dim.z);
    hdr.datatype = type32bit;
    hdr.bitpix = (type32bit == NIFTI_TYPE_FLOAT64) ? 64 : 32;
    hdr.pixdim[1] = cfg->steps.x;
    hdr.pixdim[2] = cfg->steps.y;
    hdr.pixdim[3] = cfg->steps.z;
    hdr.intent_code = NIFTI_INTENT_NONE;

    if (type32bit == NIFTI_TYPE_FLOAT32 || type32bit == NIFTI_TYPE_FLOAT64) {
        hdr.pixdim[4] = cfg->tstep * 1e6f;
    } else {
        short* mask = (short*)logval;

        for (i = 0; i < len; i++) {
            mask[(i << 1)]    = (((unsigned int*)dat)[i] & MED_MASK);
            mask[(i << 1) + 1] = (((unsigned int*)dat)[i] & DET_MASK) >> 31;
        }

        hdr.datatype = NIFTI_TYPE_UINT16;
        hdr.bitpix = 16;
        hdr.dim[1] = 2;
        hdr.dim[2] = cfg->dim.x;
        hdr.dim[3] = cfg->dim.y;
        hdr.dim[4] = cfg->dim.z;
        hdr.pixdim[4] = cfg->unitinmm;
        hdr.pixdim[1] = 1.f;
    }

    if (outputformatid == ofNifti) {
        strncpy(hdr.magic, "n+1\0", 4);
        hdr.vox_offset = (float) NII_HEADER_SIZE;
    } else {
        strncpy(hdr.magic, "ni1\0", 4);
        hdr.vox_offset = (float)0;
    }

    hdr.scl_slope = 0.f;
    hdr.xyzt_units = NIFTI_UNITS_MM | NIFTI_UNITS_USEC;

    sprintf(fname, "%s.%s", name, outputformat[outputformatid]);

    if (( fp = fopen(fname, "wb")) == NULL) {
        mcx_error(-9, "Error opening header file for write", __FILE__, __LINE__);
    }

    if (fwrite(&hdr, MIN_HEADER_SIZE, 1, fp) != 1) {
        mcx_error(-9, "Error writing header file", __FILE__, __LINE__);
    }

    if (outputformatid == ofNifti) {
        if (fwrite(&pad, 4, 1, fp) != 1) {
            mcx_error(-9, "Error writing header file extension pad", __FILE__, __LINE__);
        }

        if (fwrite(logval, (size_t)(hdr.bitpix >> 3), hdr.dim[1]*hdr.dim[2]*hdr.dim[3]*hdr.dim[4], fp) !=
                hdr.dim[1]*hdr.dim[2]*hdr.dim[3]*hdr.dim[4]) {
            mcx_error(-9, "Error writing data to file", __FILE__, __LINE__);
        }

        fclose(fp);
    } else if (outputformatid == ofAnalyze) {
        fclose(fp);  /* close .hdr file */

        sprintf(fname, "%s.img", name);

        fp = fopen(fname, "wb");

        if (fp == NULL) {
            mcx_error(-9, "Error opening img file for write", __FILE__, __LINE__);
        }

        if (fwrite(logval, (size_t)(hdr.bitpix >> 3), hdr.dim[1]*hdr.dim[2]*hdr.dim[3]*hdr.dim[4], fp) !=
                hdr.dim[1]*hdr.dim[2]*hdr.dim[3]*hdr.dim[4]) {
            mcx_error(-9, "Error writing img file", __FILE__, __LINE__);
        }

        fclose(fp);
    } else {
        mcx_error(-9, "Output format is not supported", __FILE__, __LINE__);
    }
}

/**
 * @brief Save volumetric output (fluence etc) to a binary JNIfTI/JSON/JData format file
 *
 * @param[in] dat: volumetric data to be saved
 * @param[in] len: total byte length of the data to be saved
 * @param[in] name: output file name (will append '.nii')
 * @param[in] type32bit: type of the data, only support 32bit per record
 * @param[in] outputformatid: decide if save as nii or analyze format
 * @param[in] cfg: simulation configuration
 */

void mcx_savebnii(OutputType* vol, int ndim, uint* dims, float* voxelsize, char* name, int isfloat, int iscol, mcconfig* cfg) {
    FILE* fp;
    char fname[MAX_FULL_PATH] = {'\0'};
    int affine[] = {0, 0, 1, 0, 0, 0};
    size_t datalen = sizeof(int), outputlen = 0;

    ubjw_context_t* root = NULL;
    uchar* jsonstr = NULL;

    for (int i = 0; i < ndim; i++) {
        datalen *= dims[i];
    }

    jsonstr = malloc(datalen << 1);
    root = ubjw_open_memory(jsonstr, jsonstr + (datalen << 1));

    ubjw_begin_object(root, UBJ_MIXED, 0);
    /* the "_DataInfo_" section */
    ubjw_write_key(root, "_DataInfo_");
    ubjw_begin_object(root, UBJ_MIXED, 0);
    UBJ_WRITE_KEY(root, "JNIFTIVersion", string, "0.5");
    UBJ_WRITE_KEY(root, "Comment", string, "Created by MCX (https://mcx.space)");
    UBJ_WRITE_KEY(root, "AnnotationFormat", string, "https://neurojson.org/jnifti/draft1");
    UBJ_WRITE_KEY(root, "SerialFormat", string, "https://neurojson.org/bjdata/draft2");
    ubjw_write_key(root, "Parser");
    ubjw_begin_object(root, UBJ_MIXED, 0);
    ubjw_write_key(root, "Python");
    ubjw_begin_array(root, UBJ_STRING, 2);
    ubjw_write_string(root, "https://pypi.org/project/jdata");
    ubjw_write_string(root, "https://pypi.org/project/bjdata");
    ubjw_end(root);
    ubjw_write_key(root, "MATLAB");
    ubjw_begin_array(root, UBJ_STRING, 2);
    ubjw_write_string(root, "https://github.com/NeuroJSON/jnifty");
    ubjw_write_string(root, "https://github.com/NeuroJSON/jsonlab");
    ubjw_end(root);
    ubjw_write_key(root, "JavaScript");
    ubjw_begin_array(root, UBJ_STRING, 2);
    ubjw_write_string(root, "https://www.npmjs.com/package/jda");
    ubjw_write_string(root, "https://www.npmjs.com/package/bjd");
    ubjw_end(root);
    UBJ_WRITE_KEY(root, "CPP", string, "https://github.com/NeuroJSON/json");
    UBJ_WRITE_KEY(root, "C", string, "https://github.com/NeuroJSON/ubj");
    ubjw_end(root);
    ubjw_end(root);

    /* the "NIFTIHeader" section */
    ubjw_write_key(root, "NIFTIHeader");
    ubjw_begin_object(root, UBJ_MIXED, 0);
    UBJ_WRITE_KEY(root, "NIIHeaderSize", uint16, 348);
    ubjw_write_key(root, "Dim");
    UBJ_WRITE_ARRAY(root, uint32, ndim, dims);
    UBJ_WRITE_KEY(root, "Param1", uint8, 0);
    UBJ_WRITE_KEY(root, "Param2", uint8, 0);
    UBJ_WRITE_KEY(root, "Param3", uint8, 0);
    UBJ_WRITE_KEY(root, "Intent", uint8, 0);
    UBJ_WRITE_KEY(root, "DataType", string, (sizeof(OutputType) == 8 ? "double" : (isfloat ? "single" : "uint32")));
    UBJ_WRITE_KEY(root, "BitDepth", uint8, sizeof(OutputType) * 8);
    UBJ_WRITE_KEY(root, "FirstSliceID", uint8, 0);
    ubjw_write_key(root, "VoxelSize");
    UBJ_WRITE_ARRAY(root, single, ndim, voxelsize);
    ubjw_write_key(root, "Orientation");
    ubjw_begin_object(root, UBJ_MIXED, 3);
    UBJ_WRITE_KEY(root, "x", char, 'r');
    UBJ_WRITE_KEY(root, "y", char, 'a');
    UBJ_WRITE_KEY(root, "z", char, 's');
    ubjw_end(root);
    UBJ_WRITE_KEY(root, "ScaleSlope", uint8, 1);
    UBJ_WRITE_KEY(root, "ScaleOffset", uint8, 1);
    UBJ_WRITE_KEY(root, "LastSliceID", uint32, cfg->maxgate);
    UBJ_WRITE_KEY(root, "SliceType", uint8, 1);
    ubjw_write_key(root, "Unit");
    ubjw_begin_object(root, UBJ_MIXED, 2);
    UBJ_WRITE_KEY(root, "L", string, "mm");
    UBJ_WRITE_KEY(root, "T", string, "s");
    ubjw_end(root);
    UBJ_WRITE_KEY(root, "MaxIntensity", uint32, 1);
    UBJ_WRITE_KEY(root, "MinIntensity", uint32, 0);
    UBJ_WRITE_KEY(root, "SliceTime", uint8, 0);
    UBJ_WRITE_KEY(root, "TimeOffset", uint8, 0);

    if (cfg->outputtype >= 0) {
        const char* typestr[] = {"MMC volumetric output: Fluence rate (W/mm^2)", "MMC volumetric output: Fluence (J/mm^2)",
                                 "MMC volumetric output: Voxel-wise energy deposit (J)", "MMC volumetric output: Jacobian for mua (J/mm)", "MMC volumetric output: Scattering count",
                                 "MMC volumetric output: Partial momentum transfer"
                                };
        UBJ_WRITE_KEY(root, "Description", string, typestr[(int)cfg->outputtype]);
    } else {
        UBJ_WRITE_KEY(root, "Description", string, "MMC volumetric output");
    }

    UBJ_WRITE_KEY(root, "AuxFile", string, "");
    UBJ_WRITE_KEY(root, "QForm", uint8, 0);
    UBJ_WRITE_KEY(root, "SForm", uint8, 1);
    ubjw_write_key(root, "Quatern");
    ubjw_begin_object(root, UBJ_MIXED, 3);
    UBJ_WRITE_KEY(root, "b", uint8, 0);
    UBJ_WRITE_KEY(root, "c", uint8, 0);
    UBJ_WRITE_KEY(root, "d", uint8, 0);
    ubjw_end(root);
    ubjw_write_key(root, "QuaternOffset");
    ubjw_begin_object(root, UBJ_MIXED, 3);
    UBJ_WRITE_KEY(root, "x", uint8, 0);
    UBJ_WRITE_KEY(root, "y", uint8, 0);
    UBJ_WRITE_KEY(root, "z", uint8, 0);
    ubjw_end(root);
    ubjw_write_key(root, "Affine");
    ubjw_begin_array(root, UBJ_MIXED, 0);
    UBJ_WRITE_ARRAY(root, int32, 4, affine + 2);
    UBJ_WRITE_ARRAY(root, int32, 4, affine + 1);
    UBJ_WRITE_ARRAY(root, int32, 4, affine);
    ubjw_end(root);
    UBJ_WRITE_KEY(root, "Name", string, cfg->session);
    UBJ_WRITE_KEY(root, "NIIFormat", string, "jnifti");
    ubjw_end(root);

    ubjw_write_key(root, "NIFTIData");

    /* the "NIFTIData" section stores volumetric data */
    ubjw_begin_object(root, UBJ_MIXED, 0);

    if (mcx_jdataencode(vol, ndim, dims, (sizeof(OutputType) == 8 ? "double" : (isfloat ? "single" : "uint32")), sizeof(OutputType), cfg->zipid, root, 1, iscol, cfg)) {
        MMC_ERROR(-1, "error when converting to JSON");
    }

    ubjw_end(root);
    ubjw_end(root);

    /* now save JSON to file */
    outputlen = ubjw_close_context(root);

    if (jsonstr == NULL) {
        MMC_ERROR(-1, "error when converting to JSON");
    }

    sprintf(fname, "%s.bnii", name);

    fp = fopen(fname, "wb");

    if (fp == NULL) {
        MMC_ERROR(-1, "error opening file to write");
    }

    fwrite(jsonstr, outputlen, 1, fp);
    fclose(fp);

    if (jsonstr) {
        free(jsonstr);
    }
}


/**
 * @brief Save volumetric output (fluence etc) to a JNIfTI/JSON/JData format file
 *
 * @param[in] dat: volumetric data to be saved
 * @param[in] len: total byte length of the data to be saved
 * @param[in] name: output file name (will append '.nii')
 * @param[in] type32bit: type of the data, only support 32bit per record
 * @param[in] outputformatid: decide if save as nii or analyze format
 * @param[in] cfg: simulation configuration
 */

void mcx_savejnii(OutputType* vol, int ndim, uint* dims, float* voxelsize, char* name, int isfloat, int iscol, mcconfig* cfg) {
    FILE* fp;
    char fname[MAX_FULL_PATH] = {'\0'};
    int affine[] = {0, 0, 1, 0, 0, 0};
    const char* libpy[] = {"https://pypi.org/project/jdata", "https://pypi.org/project/bjdata"};
    const char* libmat[] = {"https://github.com/NeuroJSON/jnifty", "https://github.com/NeuroJSON/jsonlab"};
    const char* libjs[] = {"https://www.npmjs.com/package/jda", "https://www.npmjs.com/package/bjd"};
    const char* libc[]  = {"https://github.com/DaveGamble/cJSON", "https://github.com/NeuroJSON/ubj"};

    cJSON* root = NULL, *hdr = NULL, *dat = NULL, *sub = NULL, *info = NULL, *parser = NULL;
    char* jsonstr = NULL;
    root = cJSON_CreateObject();

    /* the "_DataInfo_" section */
    cJSON_AddItemToObject(root, "_DataInfo_", info = cJSON_CreateObject());
    cJSON_AddStringToObject(info, "JNIFTIVersion", "0.5");
    cJSON_AddStringToObject(info, "Comment", "Created by MCX (https://mcx.space)");
    cJSON_AddStringToObject(info, "AnnotationFormat", "https://neurojson.org/jnifti/draft1");
    cJSON_AddStringToObject(info, "SerialFormat", "https://json.org");
    cJSON_AddItemToObject(info, "Parser", parser = cJSON_CreateObject());
    cJSON_AddItemToObject(parser, "Python", cJSON_CreateStringArray(libpy, 2));
    cJSON_AddItemToObject(parser, "MATLAB", cJSON_CreateStringArray(libmat, 2));
    cJSON_AddItemToObject(parser, "JavaScript", cJSON_CreateStringArray(libjs, 2));
    cJSON_AddStringToObject(parser, "CPP", "https://github.com/NeuroJSON/json");
    cJSON_AddItemToObject(parser, "C", cJSON_CreateStringArray(libc, 2));

    /* the "NIFTIHeader" section */
    cJSON_AddItemToObject(root, "NIFTIHeader", hdr = cJSON_CreateObject());
    cJSON_AddNumberToObject(hdr, "NIIHeaderSize", 348);
    cJSON_AddItemToObject(hdr, "Dim", cJSON_CreateIntArray((int*)dims, ndim));
    cJSON_AddNumberToObject(hdr, "Param1", 0);
    cJSON_AddNumberToObject(hdr, "Param2", 0);
    cJSON_AddNumberToObject(hdr, "Param3", 0);
    cJSON_AddNumberToObject(hdr, "Intent", 0);
    cJSON_AddStringToObject(hdr, "DataType", (sizeof(OutputType) == 8 ? "double" : (isfloat ? "single" : "uint32")));
    cJSON_AddNumberToObject(hdr, "BitDepth", sizeof(OutputType) * 8);
    cJSON_AddNumberToObject(hdr, "FirstSliceID", 0);
    cJSON_AddItemToObject(hdr, "VoxelSize", cJSON_CreateFloatArray(voxelsize, ndim));
    cJSON_AddItemToObject(hdr, "Orientation", sub = cJSON_CreateObject());
    cJSON_AddStringToObject(sub, "x", "r");
    cJSON_AddStringToObject(sub, "y", "a");
    cJSON_AddStringToObject(sub, "z", "s");
    cJSON_AddNumberToObject(hdr, "ScaleSlope", 1);
    cJSON_AddNumberToObject(hdr, "ScaleOffset", 0);
    cJSON_AddNumberToObject(hdr, "LastSliceID", cfg->maxgate);
    cJSON_AddNumberToObject(hdr, "SliceType", 1);
    cJSON_AddItemToObject(hdr, "Unit", sub = cJSON_CreateObject());
    cJSON_AddStringToObject(sub, "L", "mm");
    cJSON_AddStringToObject(sub, "T", "s");
    cJSON_AddNumberToObject(hdr, "MaxIntensity", 1);
    cJSON_AddNumberToObject(hdr, "MinIntensity", 0);
    cJSON_AddNumberToObject(hdr, "SliceTime", 0);
    cJSON_AddNumberToObject(hdr, "TimeOffset", 0);

    if (cfg->outputtype >= 0) {
        const char* typestr[] = {"MMC volumetric output: Fluence rate (W/mm^2)", "MMC volumetric output: Fluence (J/mm^2)",
                                 "MMC volumetric output: Voxel-wise energy deposit (J)", "MMC volumetric output: Jacobian for mua (J/mm)", "MMC volumetric output: Scattering count",
                                 "MMC volumetric output: Partial momentum transfer"
                                };
        cJSON_AddStringToObject(hdr, "Description", typestr[(int)cfg->outputtype]);
    } else {
        cJSON_AddStringToObject(hdr, "Description", "MMC volumetric output");
    }

    cJSON_AddStringToObject(hdr, "AuxFile", "");
    cJSON_AddNumberToObject(hdr, "QForm", 0);
    cJSON_AddNumberToObject(hdr, "SForm", 1);
    cJSON_AddItemToObject(hdr, "Quatern", sub = cJSON_CreateObject());
    cJSON_AddNumberToObject(sub, "b", 0);
    cJSON_AddNumberToObject(sub, "c", 0);
    cJSON_AddNumberToObject(sub, "d", 0);
    cJSON_AddItemToObject(hdr, "QuaternOffset", sub = cJSON_CreateObject());
    cJSON_AddNumberToObject(sub, "x", 0);
    cJSON_AddNumberToObject(sub, "y", 0);
    cJSON_AddNumberToObject(sub, "z", 0);
    cJSON_AddItemToObject(hdr, "Affine", sub = cJSON_CreateArray());
    cJSON_AddItemToArray(sub, cJSON_CreateIntArray(affine + 2, 4));
    cJSON_AddItemToArray(sub, cJSON_CreateIntArray(affine + 1, 4));
    cJSON_AddItemToArray(sub, cJSON_CreateIntArray(affine, 4));
    cJSON_AddStringToObject(hdr, "Name", cfg->session);
    cJSON_AddStringToObject(hdr, "NIIFormat", "jnifti");

    /* the "NIFTIData" section stores volumetric data */
    cJSON_AddItemToObject(root, "NIFTIData",   dat = cJSON_CreateObject());

    if (mcx_jdataencode(vol, ndim, dims, (sizeof(OutputType) == 8 ? "double" : (isfloat ? "single" : "uint32")), sizeof(OutputType), cfg->zipid, dat, 0, iscol, cfg)) {
        MMC_ERROR(-1, "error when converting to JSON");
    }

    /* now save JSON to file */
    jsonstr = cJSON_Print(root);

    if (jsonstr == NULL) {
        MMC_ERROR(-1, "error when converting to JSON");
    }

    sprintf(fname, "%s.jnii", name);

    fp = fopen(fname, "wt");

    if (fp == NULL) {
        MMC_ERROR(-1, "error opening file to write");
    }

    fprintf(fp, "%s\n", jsonstr);
    fclose(fp);

    if (jsonstr) {
        free(jsonstr);
    }

    if (root) {
        cJSON_Delete(root);
    }
}

/**
 * @brief Save volumetric output (fluence etc) to mc2 format binary file
 *
 * @param[in] dat: volumetric data to be saved
 * @param[in] len: total byte length of the data to be saved
 * @param[in] cfg: simulation configuration
 */

void mcx_savedata(OutputType* dat, size_t len, mcconfig* cfg, int isref) {
    FILE* fp;
    char name[MAX_FULL_PATH];
    char fname[MAX_FULL_PATH + 20];
    unsigned int glformat = GL_RGBA32F;

    if (cfg->rootpath[0])
#ifdef WIN32
        sprintf(name, "%s\\%s", cfg->rootpath, cfg->session);

#else
        sprintf(name, "%s/%s", cfg->rootpath, cfg->session);
#endif

    else {
        sprintf(name, "%s", cfg->session);
    }

    if (!isref && (cfg->outputformat == ofNifti || cfg->outputformat == ofAnalyze)) {
        mcx_savenii(dat, len, name, NIFTI_TYPE_FLOAT64, cfg->outputformat, cfg);
        return;
    } else if (cfg->outputformat == ofJNifti || cfg->outputformat == ofBJNifti) {
        uint dims[6] = {cfg->dim.x, cfg->dim.y, cfg->dim.z, cfg->maxgate, cfg->srcnum, 1}, lastdim = 5;
        float voxelsize[6] = {cfg->steps.x, cfg->steps.y, cfg->steps.z, cfg->tstep, 1, 1};

        if (cfg->method != rtBLBadouelGrid) {
            dims[0] = cfg->nodenum;
            dims[1] = cfg->maxgate;
            dims[2] = cfg->srcnum;
            dims[3] = 1;
            lastdim = 3;
            voxelsize[0] = 1.f;
            voxelsize[1] = cfg->tstep;
            voxelsize[2] = 1.f;
            voxelsize[3] = 1.f;
        }

        if (cfg->seed == SEED_FROM_FILE && (cfg->replaydet == -1 && cfg->detnum > 1)) {
            dims[lastdim] *= cfg->detnum;
        }

        if (cfg->outputformat == ofJNifti) {
            mcx_savejnii(dat, lastdim + (dims[lastdim] > 1), dims, voxelsize, name, 1, (cfg->method == rtBLBadouelGrid), cfg);
        } else {
            mcx_savebnii(dat, lastdim + (dims[lastdim] > 1), dims, voxelsize, name, 1, (cfg->method == rtBLBadouelGrid), cfg);
        }

        return;
    }

    sprintf(fname, "%s%s.%s", name, (isref ? "_dref" : ""), (isref ? "bin" : outputformat[(int)cfg->outputformat]));
    fp = fopen(fname, "wb");

    if (fp == NULL) {
        mcx_error(-2, "can not save data to disk", __FILE__, __LINE__);
    }

    if (!isref && cfg->outputformat == ofTX3) {
        fwrite(&glformat, sizeof(unsigned int), 1, fp);
        fwrite(&(cfg->dim.x), sizeof(int), 3, fp);
    }

    fwrite(dat, sizeof(OutputType), len, fp);
    fclose(fp);
}

/**
 * @brief Save detected photon data to mch format binary file
 *
 * @param[in] ppath: buffer pointing to the detected photon data (partial path etc)
 * @param[in] seeds: buffer pointing to the detected photon seed data
 * @param[in] count: number of detected photons
 * @param[in] doappend: flag if the new data is appended or write from the begining
 * @param[in] cfg: simulation configuration
 */

void mcx_savejdet(float* ppath, void* seeds, uint count, int doappend, mcconfig* cfg) {
    FILE* fp;
    char fhistory[MAX_FULL_PATH], filetag;
    cJSON* root = NULL, *obj = NULL, *hdr = NULL, *dat = NULL, *sub = NULL;
    char* jsonstr = NULL;
    int col = 0, i, j, id;

    root = cJSON_CreateObject();

    /* the "NIFTIHeader" section */
    cJSON_AddItemToObject(root, "MCXData", obj = cJSON_CreateObject());
    cJSON_AddItemToObject(obj, "Info", hdr = cJSON_CreateObject());
    cJSON_AddNumberToObject(hdr, "Version", cfg->his.version);
    cJSON_AddNumberToObject(hdr, "MediaNum", cfg->his.maxmedia);
    cJSON_AddNumberToObject(hdr, "DetNum", cfg->his.detnum);
    cJSON_AddNumberToObject(hdr, "ColumnNum", cfg->his.colcount);
    cJSON_AddNumberToObject(hdr, "TotalPhoton", cfg->his.totalphoton);
    cJSON_AddNumberToObject(hdr, "DetectedPhoton", count);
    cJSON_AddNumberToObject(hdr, "SavedPhoton", cfg->his.savedphoton);
    cJSON_AddNumberToObject(hdr, "LengthUnit", cfg->his.unitinmm);
    cJSON_AddNumberToObject(hdr, "SeedByte", cfg->his.seedbyte);
    cJSON_AddNumberToObject(hdr, "Normalizer", cfg->his.normalizer);
    cJSON_AddNumberToObject(hdr, "Repeat", cfg->his.respin);
    cJSON_AddNumberToObject(hdr, "SrcNum", cfg->his.srcnum);
    cJSON_AddNumberToObject(hdr, "SaveDetFlag", cfg->his.savedetflag);
    cJSON_AddItemToObject(hdr, "Media", sub = cJSON_CreateArray());

    for (i = 0; i < cfg->medianum; i++) {
        cJSON_AddItemToArray(sub, dat = cJSON_CreateObject());
        cJSON_AddNumberToObject(dat, "mua", cfg->prop[i].mua / cfg->unitinmm);
        cJSON_AddNumberToObject(dat, "mus", cfg->prop[i].mus / cfg->unitinmm);
        cJSON_AddNumberToObject(dat, "g",   cfg->prop[i].g);
        cJSON_AddNumberToObject(dat, "n",   cfg->prop[i].n);
    }

    if (cfg->his.detected == 0  && cfg->his.savedphoton) {
        char colnum[] = {1, 3, 1};
        char* dtype[] = {"uint32", "single", "single"};
        char* dname[] = {"photonid", "p", "w0"};
        cJSON_AddItemToObject(obj, "Trajectory", dat = cJSON_CreateObject());

        for (id = 0; id < sizeof(colnum); id++) {
            uint dims[2] = {count, colnum[id]};
            float* buf = (float*)calloc(dims[0] * dims[1], sizeof(float));

            for (i = 0; i < dims[0]; i++)
                for (j = 0; j < dims[1]; j++) {
                    buf[i * dims[1] + j] = ppath[i * cfg->his.colcount + col + j];
                }

            cJSON_AddItemToObject(dat, dname[id], sub = cJSON_CreateObject());

            if (mcx_jdataencode(buf, 2, dims, dtype[id], 4, cfg->zipid, sub, 0, 1, cfg)) {
                MMC_ERROR(-1, "error when converting to JSON");
            }

            free(buf);
            col += dims[1];
        }
    } else {
        char colnum[] = {1, cfg->his.maxmedia, cfg->his.maxmedia, cfg->his.maxmedia, 3, 3, 1};
        char* dtype[] = {"uint32", "uint32", "single", "single", "single", "single", "single"};
        char* dname[] = {"detid", "nscat", "ppath", "mom", "p", "v", "w0"};

        cJSON_AddItemToObject(obj, "PhotonData", dat = cJSON_CreateObject());

        for (id = 0; id < sizeof(colnum); id++) {
            if ((cfg->savedetflag >> id) & 0x1) {
                uint dims[2] = {count, colnum[id]};
                void* val = NULL;
                float* fbuf = NULL;
                uint*  ibuf = NULL;

                if (!strcmp(dtype[id], "uint32")) {
                    ibuf = (uint*)calloc(dims[0] * dims[1], sizeof(uint));

                    for (i = 0; i < dims[0]; i++)
                        for (j = 0; j < dims[1]; j++) {
                            ibuf[i * dims[1] + j] = ppath[i * cfg->his.colcount + col + j];
                        }

                    val = (void*)ibuf;
                } else {
                    fbuf = (float*)calloc(dims[0] * dims[1], sizeof(float));

                    for (i = 0; i < dims[0]; i++)
                        for (j = 0; j < dims[1]; j++) {
                            fbuf[i * dims[1] + j] = ppath[i * cfg->his.colcount + col + j];
                        }

                    val = (void*)fbuf;
                }

                cJSON_AddItemToObject(dat, dname[id], sub = cJSON_CreateObject());

                if (mcx_jdataencode(val, 2, dims, dtype[id], 4, cfg->zipid, sub, 0, 1, cfg)) {
                    MMC_ERROR(-1, "error when converting to JSON");
                }

                free(val);
                col += dims[1];
            }
        }
    }

    if (cfg->issaveseed && seeds != NULL) {
        uint dims[2] = {count, cfg->his.seedbyte};
        cJSON_AddItemToObject(dat, "seed", sub = cJSON_CreateObject());

        if (mcx_jdataencode(seeds, 2, dims, "uint8", 1, cfg->zipid, sub, 0, 1, cfg)) {
            MMC_ERROR(-1, "error when converting to JSON");
        }
    }

    /* now save JSON to file */
    jsonstr = cJSON_Print(root);

    if (jsonstr == NULL) {
        MMC_ERROR(-1, "error when converting to JSON");
    }

    filetag = ((cfg->his.detected == 0  && cfg->his.savedphoton) ? 't' : 'h');

    if (cfg->rootpath[0]) {
        sprintf(fhistory, "%s%c%s_%s.jdat", cfg->rootpath, pathsep, cfg->session, (filetag == 't' ? "traj" : "detp"));
    } else {
        sprintf(fhistory, "%s_%s.jdat", cfg->session, (filetag == 't' ? "traj" : "detp"));
    }

    if (doappend) {
        fp = fopen(fhistory, "at");
    } else {
        fp = fopen(fhistory, "wt");
    }

    if (fp == NULL) {
        MMC_ERROR(-2, "can not save data to disk");
    }

    fprintf(fp, "%s\n", jsonstr);
    fclose(fp);

    if (jsonstr) {
        free(jsonstr);
    }

    if (root) {
        cJSON_Delete(root);
    }
}

#endif

/**
 * @brief Print a message to the console or a log file
 *
 * @param[in] cfg: simulation configuration
 * @param[in] str: a string to be printed
 */

void mcx_printlog(mcconfig* cfg, char* str) {
    if (cfg->flog > 0) { /*stdout is 1*/
        MMC_FPRINTF(cfg->flog, "%s\n", str);
    }
}

/**
 * @brief Normalize the solution by multiplying a scaling factor
 *
 * @param[in,out] field: volumetric data before normalization
 * @param[in] scale: the scaling factor (or normalization factor) to be applied
 * @param[in] fieldlen: the length (floating point) of elements in the volume
 */

void mcx_normalize(float field[], float scale, int fieldlen) {
    int i;

    for (i = 0; i < fieldlen; i++) {
        field[i] *= scale;
    }
}

/**
 * @brief Error reporting function
 *
 * @param[in] id: a single integer for the types of the error
 * @param[in] msg: the error message string
 * @param[in] file: the unit file name where this error is raised
 * @param[in] linenum: the line number in the file where this error is raised
 */

void mcx_error(const int id, const char* msg, const char* file, const int linenum) {
    #pragma omp critical
    {
#ifdef MCX_CONTAINER
        mmc_throw_exception(id, msg, file, linenum);
#else

        if (id == MMC_INFO) {
            MMC_FPRINTF(stdout, "%s\n", msg);
        } else {
            MMC_FPRINTF(stdout, S_RED"\nMMC ERROR(%d):%s in unit %s:%d\n"S_RESET, id, msg, file, linenum);
        }

        exit(id);
#endif
    }
}

/**
 * @brief Function to test return value and raise an error
 *
 * @param[in] ret: function return value, non-zero means an error
 * @param[in] file: the unit file name where this error is raised
 * @param[in] linenum: the line number in the file where this error is raised
 */

void mcx_assert(const int ret, const char* file, const int linenum) {
    if (!ret) {
        mcx_error(ret, "input error", file, linenum);
    }
}

#ifndef MCX_CONTAINER

/**
 * @brief Read simulation settings from a configuration file (.inp or .json)
 *
 * @param[in] fname: the name of the input file (.inp or .json)
 * @param[in] cfg: simulation configuration
 */

void mcx_readconfig(char* fname, mcconfig* cfg) {
    if (fname[0] == 0) {
        mcx_loadconfig(stdin, cfg);

        if (cfg->session[0] == '\0') {
            strcpy(cfg->session, "default");
        }
    } else {
        FILE* fp = fopen(fname, "rt");

        if (fp == NULL) {
            MMC_ERROR(-2, "can not load the specified config file");
        }

        if (strstr(fname, ".json") != NULL) {
            char* jbuf;
            int len;

            fclose(fp);
            fp = fopen(fname, "rb");
            fseek (fp, 0, SEEK_END);
            len = ftell(fp) + 1;
            jbuf = (char*)malloc(len);
            rewind(fp);

            if (fread(jbuf, len - 1, 1, fp) != 1) {
                MMC_ERROR(-2, "reading input file is terminated");
            }

            jbuf[len - 1] = '\0';

            if (mcx_loadfromjson(jbuf, cfg)) {
                fclose(fp);
                free(jbuf);
                MMC_ERROR(-9, "invalid JSON input file");
            }

            free(jbuf);
        } else {
            mcx_loadconfig(fp, cfg);
        }

        fclose(fp);

        if (cfg->session[0] == '\0') {
            strncpy(cfg->session, fname, MAX_SESSION_LENGTH - 1);
        }
    }
}

/**
 * @brief Load a .json input file into memory
 *
 * This function loads a JSON file using cJSON
 *
 * @param[out] jbuf: json data structure pointer
 * @param[in] cfg: simulation configuration
 */

int mcx_loadfromjson(char* jbuf, mcconfig* cfg) {
    cJSON* jroot;
    jroot = cJSON_Parse(jbuf);

    if (jroot) {
        mcx_loadjson(jroot, cfg);
        cJSON_Delete(jroot);
    } else {
        char* ptrold, *ptr = (char*)cJSON_GetErrorPtr();

        if (ptr) {
            ptrold = strstr(jbuf, ptr);
        }

        if (ptr && ptrold) {
            char* offs = (ptrold - jbuf >= 50) ? ptrold - 50 : jbuf;

            while (offs < ptrold) {
                MMC_FPRINTF(stderr, "%c", *offs);
                offs++;
            }

            MMC_FPRINTF(stderr, S_RED"<error>%.50s\n"S_RESET, ptrold);
        }

        return 1;
    }

    return 0;
}

/**
 * @brief Load user inputs from a .json input file
 *
 * This function loads user input from a JSON format in a .json extension
 *
 * @param[out] root: json data structure pointer
 * @param[in] cfg: simulation configuration
 */

int mcx_loadjson(cJSON* root, mcconfig* cfg) {
    int i;
    cJSON* Mesh, *Optode, *Forward, *Session, *Domain, *tmp, *subitem;

    Mesh    = cJSON_GetObjectItem(root, "Mesh");

    if (!Mesh) {
        Mesh    = cJSON_GetObjectItem(root, "Shapes");
    }

    Optode  = cJSON_GetObjectItem(root, "Optode");
    Session = cJSON_GetObjectItem(root, "Session");
    Forward = cJSON_GetObjectItem(root, "Forward");
    Domain  = cJSON_GetObjectItem(root, "Domain");

    if (Mesh) {
        subitem = FIND_JSON_OBJ("MeshID", "Mesh.MeshID", Mesh);

        if (subitem) {
            strncpy(cfg->meshtag, FIND_JSON_KEY("MeshID", "Mesh.MeshID", Mesh, (MMC_ERROR(-1, "You must specify mesh files"), ""), valuestring), MAX_SESSION_LENGTH - 1);
        }

        //< reading mesh node and elem mesh data from JSON file
        {
            int ndim;
            uint dims[3] = {1, 1, 1};
            char* type = NULL;

            subitem = FIND_JSON_OBJ("MeshNode", "Mesh.MeshNode", Mesh);

            if (subitem) {
                if (cfg->node) {
                    free(cfg->node);
                    cfg->node = NULL;
                }

                if (cJSON_IsArray(subitem)) {
                    cfg->nodenum = cJSON_GetArraySize(subitem);
                    subitem = subitem->child;
                    cfg->node = (FLOAT3*)malloc(sizeof(FLOAT3) * cfg->nodenum);

                    for (int i = 0; i < cfg->nodenum; i++) {
                        if (cJSON_GetArraySize(subitem) != 3) {
                            MMC_ERROR(-1, "Each element in MeshNode must have 3 numbers");
                        }

                        cfg->node[i].x = subitem->child->valuedouble;
                        cfg->node[i].y = subitem->child->next->valuedouble;
                        cfg->node[i].z = subitem->child->next->next->valuedouble;
                        subitem = subitem->next;
                    }
                } else {
                    mcx_jdatadecode((void**)&cfg->node, &ndim, dims, 2, &type, subitem, cfg);

                    if (strcmp(type, "single") || ndim != 2 || (ndim >= 2 && dims[1] != 3)) {
                        if (cfg->node) {
                            free(cfg->node);
                        }

                        MMC_ERROR(-1, "Mesh.MeshNode JData-annotated array must be in the 'single' format");
                    } else {
                        cfg->nodenum = dims[0];
                    }
                }
            }

            subitem = FIND_JSON_OBJ("MeshElem", "Mesh.MeshElem", Mesh);

            if (subitem) {
                if (cfg->elem) {
                    free(cfg->elem);
                    cfg->elem = NULL;
                }

                if (cJSON_IsArray(subitem)) {
                    cfg->elemnum = cJSON_GetArraySize(subitem);
                    subitem = subitem->child;
                    cfg->elemlen = cJSON_GetArraySize(subitem);
                    cfg->elem = (int*)malloc(sizeof(int) * cfg->elemnum * cfg->elemlen);

                    for (int i = 0; i < cfg->elemnum; i++) {
                        if (cJSON_GetArraySize(subitem) != 5) {
                            MMC_ERROR(-1, "Each element in MeshElem must have 5 integers");
                        }

                        tmp = subitem->child;

                        for (int j = 0; j < 5; j++) {
                            cfg->elem[ i * cfg->elemlen + j ] = tmp->valueint;
                            tmp = tmp->next;
                        }

                        subitem = subitem->next;
                    }

                    cfg->elemlen--; // to make cfg->elemlen and mesh->elemlen the same - cfg->elem contains 1 extra column for mesh->type
                } else {
                    mcx_jdatadecode((void**)&cfg->elem, &ndim, dims, 2, &type, subitem, cfg);

                    if (!strstr(type, "int32") || ndim != 2 || (ndim >= 2 && dims[1] != 5 && dims[1] != 11)) {
                        if (cfg->elem) {
                            free(cfg->elem);
                        }

                        MMC_ERROR(-1, "Mesh.MeshElem JData-annotated array must be in the 'int32' format");
                    } else {
                        cfg->elemnum = dims[0];
                        cfg->elemlen = dims[1] - 1;
                    }
                }
            }

            subitem = FIND_JSON_OBJ("MeshROI", "Mesh.MeshROI", Mesh);

            if (subitem) {
                if (cfg->roidata) {
                    free(cfg->roidata);
                    cfg->roidata = NULL;
                }

                if (cJSON_IsArray(subitem)) {
                    int collen, rowlen = cJSON_GetArraySize(subitem);
                    subitem = subitem->child;

                    if (cJSON_IsArray(subitem)) {
                        collen = cJSON_GetArraySize(subitem);
                    } else {
                        collen = rowlen;
                        rowlen = 1;
                    }

                    cfg->roitype = (collen == 6) ? rtEdge : ((collen == 1 || rowlen == 1) ? rtNode : (collen == 4 ? rtFace : rtNone));
                    cfg->roidata = (float*)malloc(sizeof(float) * rowlen * collen);
                    cfg->implicit = (cfg->roitype != rtNone) + (cfg->roitype == rtFace);

                    for (int i = 0; i < rowlen; i++) {
                        if (cJSON_GetArraySize(subitem) != collen) {
                            MMC_ERROR(-1, "Each element in MeshROI must have the same length");
                        }

                        tmp = subitem->child;

                        for (int j = 0; j < collen; j++) {
                            cfg->roidata[ i * collen + j ] = tmp->valuedouble;
                            tmp = tmp->next;
                        }

                        subitem = subitem->next;
                    }
                } else {
                    mcx_jdatadecode((void**)&cfg->roidata, &ndim, dims, 2, &type, subitem, cfg);

                    if (strcmp(type, "single") || ndim > 2 || (ndim >= 2 && dims[1] != 6 && dims[1] != 4)) {
                        free(cfg->roidata);
                        MMC_ERROR(-1, "Mesh.MeshROI JData-annotated array must be in the 'single' format");
                    } else {
                        cfg->roitype = (dims[1] == 6) ? rtEdge : ((ndim == 1 || dims[0] == 1 || dims[1] == 1) ? rtNode : (dims[1] == 4 ? rtFace : rtNone));
                    }
                }
            }
        }

        cfg->e0 = FIND_JSON_KEY("InitElem", "Mesh.InitElem", Mesh, (MMC_ERROR(-1, "InitElem must be given"), 0.0), valueint);

        if (!flagset['u']) {
            cfg->unitinmm = FIND_JSON_KEY("LengthUnit", "Mesh.LengthUnit", Mesh, 1.0, valuedouble);
        }
    }

    if (Domain) {
        cJSON* val, *meds = FIND_JSON_OBJ("Media", "Domain.Media", Domain);

        if (meds) {
            cJSON* med = meds->child;

            if (med) {
                cfg->medianum = cJSON_GetArraySize(meds);

                if (cfg->prop) {
                    free(cfg->prop);
                }

                cfg->prop = (medium*)malloc(sizeof(medium) * cfg->medianum);

                for (i = 0; i < cfg->medianum; i++) {
                    if (cJSON_IsObject(med)) {
                        val = FIND_JSON_OBJ("mua", (MMC_ERROR(-1, "You must specify absorption coeff, default in 1/mm"), ""), med);

                        if (val) {
                            cfg->prop[i].mua = val->valuedouble;
                        }

                        val = FIND_JSON_OBJ("mus", (MMC_ERROR(-1, "You must specify scattering coeff, default in 1/mm"), ""), med);

                        if (val) {
                            cfg->prop[i].mus = val->valuedouble;
                        }

                        val = FIND_JSON_OBJ("g", (MMC_ERROR(-1, "You must specify anisotropy [0-1]"), ""), med);

                        if (val) {
                            cfg->prop[i].g = val->valuedouble;
                        }

                        val = FIND_JSON_OBJ("n", (MMC_ERROR(-1, "You must specify refractive index"), ""), med);

                        if (val) {
                            cfg->prop[i].n = val->valuedouble;
                        }
                    } else if (cJSON_IsArray(med)) {
                        cfg->prop[i].mua = med->child->valuedouble;
                        cfg->prop[i].mus = med->child->next->valuedouble;
                        cfg->prop[i].g = med->child->next->next->valuedouble;
                        cfg->prop[i].n = med->child->next->next->next->valuedouble;
                    } else {
                        MMC_ERROR(-1, "Session.Media must be either an array of objects or array of 4-elem numerical arrays");
                    }

                    med = med->next;

                    if (med == NULL) {
                        break;
                    }
                }
            }
        }

        val = FIND_JSON_OBJ("Step", "Domain.Step", Domain);

        if (val) {
            if (cJSON_GetArraySize(val) >= 1) {
                cfg->steps.x = val->child->valuedouble;
                cfg->steps.y = cfg->steps.x;
                cfg->steps.z = cfg->steps.x;
            } else {
                MMC_ERROR(-1, "Domain::Step has incorrect element numbers");
            }
        }

        if (!flagset['u']) {
            cfg->unitinmm = FIND_JSON_KEY("LengthUnit", "Domain.LengthUnit", Domain, 1.0, valuedouble);
        }
    }

    if (Optode) {
        cJSON* dets, *src = FIND_JSON_OBJ("Source", "Optode.Source", Optode);

        if (src) {
            subitem = FIND_JSON_OBJ("Pos", "Optode.Source.Pos", src);

            if (subitem) {
                cfg->srcpos.x = subitem->child->valuedouble;
                cfg->srcpos.y = subitem->child->next->valuedouble;
                cfg->srcpos.z = subitem->child->next->next->valuedouble;
            }

            subitem = FIND_JSON_OBJ("Dir", "Optode.Source.Dir", src);

            if (subitem) {
                cfg->srcdir.x = subitem->child->valuedouble;
                cfg->srcdir.y = subitem->child->next->valuedouble;
                cfg->srcdir.z = subitem->child->next->next->valuedouble;

                if (subitem->child->next->next->next) {
                    cfg->srcdir.w = subitem->child->next->next->next->valuedouble;
                }
            }

            subitem = FIND_JSON_OBJ("Type", "Optode.Source.Type", src);

            if (subitem) {
                cfg->srctype = mcx_keylookup(subitem->valuestring, srctypeid);
            }

            subitem = FIND_JSON_OBJ("Param1", "Optode.Source.Param1", src);

            if (subitem && cJSON_GetArraySize(subitem) == 4) {
                cfg->srcparam1.x = subitem->child->valuedouble;

                if (subitem->child->next) {
                    cfg->srcparam1.y = subitem->child->next->valuedouble;

                    if (subitem->child->next->next) {
                        cfg->srcparam1.z = subitem->child->next->next->valuedouble;

                        if (subitem->child->next->next->next) {
                            cfg->srcparam1.w = subitem->child->next->next->next->valuedouble;
                        }
                    }
                }
            }

            subitem = FIND_JSON_OBJ("Param2", "Optode.Source.Param2", src);

            if (subitem && cJSON_GetArraySize(subitem) == 4) {
                cfg->srcparam2.x = subitem->child->valuedouble;

                if (subitem->child->next) {
                    cfg->srcparam2.y = subitem->child->next->valuedouble;

                    if (subitem->child->next->next) {
                        cfg->srcparam2.z = subitem->child->next->next->valuedouble;

                        if (subitem->child->next->next->next) {
                            cfg->srcparam2.w = subitem->child->next->next->next->valuedouble;
                        }
                    }
                }
            }

            subitem = FIND_JSON_OBJ("Pattern", "Optode.Source.Pattern", src);

            if (subitem) {
                if (FIND_JSON_OBJ("_ArrayZipData_", "Optode.Source.Pattern._ArrayZipData_", subitem)) {
                    int ndim;
                    uint dims[3] = {1, 1, 1};
                    char* type = NULL;

                    if (cfg->srcpattern) {
                        free(cfg->srcpattern);
                    }

                    mcx_jdatadecode((void**)&cfg->srcpattern, &ndim, dims, 3, &type, subitem, cfg);

                    if (strcmp(type, "single")) {
                        if (cfg->srcpattern) {
                            free(cfg->srcpattern);
                        }

                        MMC_ERROR(-1, "Optode.Source.Pattern JData-annotated array must be in the 'single' format");
                    }

                    if (ndim == 3 && dims[2] > 1 && dims[0] > 1 && cfg->srctype == MCX_SRC_PATTERN) {
                        cfg->srcnum = dims[0];
                    }

                    mcx_convertrow2col(cfg->srcpattern, (uint3*)dims);
                } else {
                    int nx = FIND_JSON_KEY("Nx", "Optode.Source.Pattern.Nx", subitem, 0, valueint);
                    int ny = FIND_JSON_KEY("Ny", "Optode.Source.Pattern.Ny", subitem, 0, valueint);
                    int nz = FIND_JSON_KEY("Nz", "Optode.Source.Pattern.Nz", subitem, 1, valueint);

                    if (nx > 0 && ny > 0) {
                        cJSON* pat = FIND_JSON_OBJ("Data", "Optode.Source.Pattern.Data", subitem);

                        if (pat && pat->child) {
                            int i;
                            pat = pat->child;

                            if (cfg->srcpattern) {
                                free(cfg->srcpattern);
                            }

                            cfg->srcpattern = (float*)calloc(nx * ny * nz * cfg->srcnum, sizeof(float));

                            for (i = 0; i < nx * ny * nz * cfg->srcnum; i++) {
                                if (pat == NULL) {
                                    MMC_ERROR(-1, "Incomplete pattern data");
                                }

                                cfg->srcpattern[i] = pat->valuedouble;
                                pat = pat->next;
                            }
                        } else if (pat) {
                            FILE* fid = fopen(pat->valuestring, "rb");

                            if (fid != NULL) {
                                if (cfg->srcpattern) {
                                    free(cfg->srcpattern);
                                }

                                cfg->srcpattern = (float*)calloc(nx * ny * nz * cfg->srcnum, sizeof(float));

                                if (fread((void*)cfg->srcpattern, sizeof(float) * nx * ny * nz * cfg->srcnum, 1, fid) != 1) {
                                    fclose(fid);
                                    MMC_ERROR(-1, "Fail to read pattern input file");
                                }

                                fclose(fid);
                            }
                        }
                    }
                }
            }
        }

        dets = FIND_JSON_OBJ("Detector", "Optode.Detector", Optode);

        if (dets) {
            cJSON* det = dets;

            if (!FIND_JSON_OBJ("Pos", "Optode.Detector.Pos", dets)) {
                det = dets->child;
            }

            if (det) {
                cfg->detnum = cJSON_GetArraySize(dets);
                cfg->detpos = (float4*)malloc(sizeof(float4) * cfg->detnum);

                for (i = 0; i < cfg->detnum; i++) {
                    cJSON* pos = dets, *rad = NULL;
                    rad = FIND_JSON_OBJ("R", "Optode.Detector.R", det);

                    if (cJSON_GetArraySize(det) == 2) {
                        pos = FIND_JSON_OBJ("Pos", "Optode.Detector.Pos", det);
                    }

                    if (pos) {
                        cfg->detpos[i].x = pos->child->valuedouble;
                        cfg->detpos[i].y = pos->child->next->valuedouble;
                        cfg->detpos[i].z = pos->child->next->next->valuedouble;
                    }

                    if (rad) {
                        cfg->detpos[i].w = rad->valuedouble;
                    }

                    det = det->next;

                    if (det == NULL) {
                        break;
                    }
                }
            }
        }
    }

    if (Session) {
        char val[2] = {'\0', '\0'};
        cJSON* ck;

        if (!flagset['E']) {
            cfg->seed = FIND_JSON_KEY("RNGSeed", "Session.RNGSeed", Session, -1, valueint);
        }

        if (!flagset['n']) {
            cfg->nphoton = FIND_JSON_KEY("Photons", "Session.Photons", Session, 0, valueint);
        }

        if (cfg->session[0] == '\0') {
            strncpy(cfg->session, FIND_JSON_KEY("ID", "Session.ID", Session, "default", valuestring), MAX_SESSION_LENGTH);
        }

        if (!flagset['b']) {
            cfg->isreflect = FIND_JSON_KEY("DoMismatch", "Session.DoMismatch", Session, cfg->isreflect, valueint);
        }

        if (!flagset['S']) {
            cfg->issave2pt = FIND_JSON_KEY("DoSaveVolume", "Session.DoSaveVolume", Session, cfg->issave2pt, valueint);
        }

        if (!flagset['U']) {
            cfg->isnormalized = FIND_JSON_KEY("DoNormalize", "Session.DoNormalize", Session, cfg->isnormalized, valueint);
        }

        if (!flagset['d']) {
            cfg->issavedet = FIND_JSON_KEY("DoPartialPath", "Session.DoPartialPath", Session, cfg->issavedet, valueint);
        }

        if (!flagset['V']) {
            cfg->isspecular = FIND_JSON_KEY("DoSpecular", "Session.DoSpecular", Session, cfg->isspecular, valueint);
        }

        if (!flagset['m']) {
            cfg->ismomentum = FIND_JSON_KEY("DoDCS", "Session.DoDCS", Session, cfg->ismomentum, valueint);
        }

        if (!flagset['x']) {
            cfg->issaveexit = FIND_JSON_KEY("DoSaveExit", "Session.DoSaveExit", Session, cfg->issaveexit, valueint);
        }

        if (!flagset['q']) {
            cfg->issaveseed = FIND_JSON_KEY("DoSaveSeed", "Session.DoSaveSeed", Session, cfg->issaveseed, valueint);
        }

        if (!flagset['C']) {
            cfg->basisorder = FIND_JSON_KEY("BasisOrder", "Session.BasisOrder", Session, cfg->basisorder, valueint);
        }

        if (!flagset['F']) {
            cfg->outputformat = mcx_keylookup((char*)FIND_JSON_KEY("OutputFormat", "Session.OutputFormat", Session, "ascii", valuestring), outputformat);
        }

        if (cfg->outputformat < 0) {
            MMC_ERROR(-2, "the specified output format is not recognized");
        }

        if (!flagset['D']) {
            if (FIND_JSON_KEY("DebugFlag", "Session.DebugFlag", Session, "", valuestring)) {
                cfg->debuglevel = mcx_parsedebugopt(FIND_JSON_KEY("DebugFlag", "Session.DebugFlag", Session, "", valuestring), debugflag);
            } else {
                cfg->debuglevel = FIND_JSON_KEY("DebugFlag", "Session.DebugFlag", Session, 0, valueint);
            }
        }

        if (!flagset['M']) {
            strncpy(val, FIND_JSON_KEY("RayTracer", "Session.RayTracer", Session, raytracing + cfg->method, valuestring), 1);

            if (mcx_lookupindex(val, raytracing)) {
                MMC_ERROR(-2, "the specified ray-tracing method is not recognized");
            }

            cfg->method = val[0];
        }

        if (!flagset['O']) {
            strncpy(val, FIND_JSON_KEY("OutputType", "Session.OutputType", Session, outputtype + cfg->outputtype, valuestring), 1);

            if (mcx_lookupindex(val, outputtype)) {
                MMC_ERROR(-2, "the specified output data type is not recognized");
            }

            cfg->outputtype = val[0];
        }

        ck = FIND_JSON_OBJ("Checkpoints", "Session.Checkpoints", Session);

        if (ck) {
            int num = MIN(cJSON_GetArraySize(ck), MAX_CHECKPOINT);
            ck = ck->child;

            for (i = 0; i < num; i++) {
                cfg->checkpt[i] = ck->valueint;
                ck = ck->next;

                if (ck == NULL) {
                    break;
                }
            }
        }
    }

    if (Forward) {
        cfg->tstart = FIND_JSON_KEY("T0", "Forward.T0", Forward, 0.0, valuedouble);
        cfg->tend  = FIND_JSON_KEY("T1", "Forward.T1", Forward, 0.0, valuedouble);
        cfg->tstep = FIND_JSON_KEY("Dt", "Forward.Dt", Forward, 0.0, valuedouble);
        cfg->nout = FIND_JSON_KEY("N0", "Forward.N0", Forward, cfg->nout, valuedouble);

        cfg->maxgate = (int)((cfg->tend - cfg->tstart) / cfg->tstep + 0.5);
    }

    if (cfg->meshtag[0] == '\0' && cfg->nodenum == 0) {
        MMC_ERROR(-1, "You must specify mesh files");
    }

    if (cfg->e0 == 0) {
        MMC_ERROR(-1, "InitElem must be given");
    }

    if (cfg->isdumpjson == 3) {
        mcx_savejdata(cfg->jsonfile, cfg);
        exit(0);
    }

    return 0;
}

/**
 * @brief Write simulation settings to an inp file
 *
 * @param[in] fname: the name of the output file
 * @param[in] cfg: simulation configuration
 */

void mcx_writeconfig(char* fname, mcconfig* cfg) {
    if (fname[0] == 0) {
        mcx_saveconfig(stdout, cfg);
    } else {
        FILE* fp = fopen(fname, "wt");

        if (fp == NULL) {
            MMC_ERROR(-2, "can not write to the specified config file");
        }

        mcx_saveconfig(fp, cfg);
        fclose(fp);
    }
}

/**
 * @brief Load user inputs from a .inp input file
 *
 * This function loads user input from a simple text input format in a .inp extension
 *
 * @param[in] in: file handle to the .inp file
 * @param[in] cfg: simulation configuration
 */

void mcx_loadconfig(FILE* in, mcconfig* cfg) {
    int i, gates, srctype, itmp;
    size_t nphoton;
    float dtmp;
    char comment[MAX_FULL_PATH], *comm, srctypestr[MAX_SESSION_LENGTH] = {'\0'};

    if (in == stdin) {
        MMC_FPRINTF(stdout, "Please specify the total number of photons: [1000000]\n\t");
    }

    MMC_ASSERT(fscanf(in, "%zu", &(nphoton) ) == 1);

    if (cfg->nphoton == 0) {
        cfg->nphoton = nphoton;
    }

    comm = fgets(comment, MAX_PATH_LENGTH, in);

    if (in == stdin) {
        MMC_FPRINTF(stdout, ">> %zu\nPlease specify the random number generator seed: [123456789]\n\t", cfg->nphoton);
    }

    if (cfg->seed == 0x623F9A9E) {
        MMC_ASSERT(fscanf(in, "%d", &(cfg->seed) ) == 1);
    } else {
        MMC_ASSERT(fscanf(in, "%d", &itmp ) == 1);
    }

    comm = fgets(comment, MAX_PATH_LENGTH, in);

    if (in == stdin) {
        MMC_FPRINTF(stdout, ">> %d\nPlease specify the position of the source: [10 10 5]\n\t", cfg->seed);
    }

    MMC_ASSERT(fscanf(in, "%f %f %f", &(cfg->srcpos.x), &(cfg->srcpos.y), &(cfg->srcpos.z) ) == 3);
    comm = fgets(comment, MAX_PATH_LENGTH, in);

    if (in == stdin)
        MMC_FPRINTF(stdout, ">> %f %f %f\nPlease specify the normal direction of the source: [0 0 1]\n\t",
                    cfg->srcpos.x, cfg->srcpos.y, cfg->srcpos.z);

    MMC_ASSERT(fscanf(in, "%f %f %f", &(cfg->srcdir.x), &(cfg->srcdir.y), &(cfg->srcdir.z)));
    comm = fgets(comment, MAX_PATH_LENGTH, in);

    if (comm != NULL && sscanf(comm, "%f", &dtmp) == 1) {
        cfg->srcdir.w = dtmp;
    }

    if (in == stdin)
        MMC_FPRINTF(stdout, ">> %f %f %f %f\nPlease specify the time gates in seconds (start end step) [0.0 1e-9 1e-10]\n\t",
                    cfg->srcdir.x, cfg->srcdir.y, cfg->srcdir.z, cfg->srcdir.w);

    MMC_ASSERT(fscanf(in, "%f %f %f", &(cfg->tstart), &(cfg->tend), &(cfg->tstep) ) == 3);
    comm = fgets(comment, MAX_PATH_LENGTH, in);

    if (in == stdin)
        MMC_FPRINTF(stdout, ">> %f %f %f\nPlease specify the mesh file key {node,elem,velem,facenb}_key.dat :\n\t",
                    cfg->tstart, cfg->tend, cfg->tstep);

    if (cfg->tstart > cfg->tend || cfg->tstep == 0.f) {
        MMC_ERROR(-9, "incorrect time gate settings");
    }

    if (cfg->tstep > cfg->tend - cfg->tstart) {
        cfg->tstep = cfg->tend - cfg->tstart;
    }

    gates = (int)((cfg->tend - cfg->tstart) / cfg->tstep + 0.5);
    /*if(cfg->maxgate>gates)*/
    cfg->maxgate = gates;

    MMC_ASSERT(fscanf(in, "%s", cfg->meshtag) == 1);

    if (cfg->rootpath[0]) {
#ifdef WIN32
        sprintf(comment, "%s\\%s", cfg->rootpath, cfg->meshtag);
#else
        sprintf(comment, "%s/%s", cfg->rootpath, cfg->meshtag);
#endif
        memcpy(cfg->meshtag, comment, MAX_SESSION_LENGTH);
    }

    comm = fgets(comment, MAX_PATH_LENGTH, in);

    if (in == stdin) {
        MMC_FPRINTF(stdout, ">> %s\nPlease specify the index to the tetrahedral element enclosing the source [start from 1]:\n\t", cfg->meshtag);
    }

    MMC_ASSERT(fscanf(in, "%d", &(cfg->e0)) == 1);
    comm = fgets(comment, MAX_PATH_LENGTH, in);

    if (in == stdin) {
        MMC_FPRINTF(stdout, ">> %d\nPlease specify the total number of detectors and detector diameter (in mm):\n\t", cfg->e0);
    }

    MMC_ASSERT(fscanf(in, "%d %f", &(cfg->detnum), &(cfg->detradius)) == 2);
    comm = fgets(comment, MAX_PATH_LENGTH, in);

    if (in == stdin) {
        MMC_FPRINTF(stdout, ">> %d %f\n", cfg->detnum, cfg->detradius);
    }

    cfg->detpos = (float4*)malloc(sizeof(float4) * cfg->detnum);

    if (cfg->issavedet) {
        cfg->issavedet = (cfg->detpos > 0);
    }

    for (i = 0; i < cfg->detnum; i++) {
        if (in == stdin) {
            MMC_FPRINTF(stdout, "Please define detector #%d: x,y,z (in mm): [5 5 5 1]\n\t", i);
        }

        MMC_ASSERT(fscanf(in, "%f %f %f", &(cfg->detpos[i].x), &(cfg->detpos[i].y), &(cfg->detpos[i].z)) == 3);
        comm = fgets(comment, MAX_PATH_LENGTH, in);

        if (comm != NULL && sscanf(comm, "%f", &dtmp) == 1) {
            cfg->detpos[i].w = dtmp;
        } else {
            cfg->detpos[i].w = cfg->detradius;
        }

        if (in == stdin) {
            MMC_FPRINTF(stdout, ">> %f %f %f\n", cfg->detpos[i].x, cfg->detpos[i].y, cfg->detpos[i].z);
        }
    }

    if (in == stdin) {
        MMC_FPRINTF(stdout, "Please specify the source type [pencil|isotropic|cone|gaussian|planar|pattern|fourier|arcsine|disk|fourierx|fourierx2d|zgaussian|line|slit]:\n\t");
    }

    if (fscanf(in, "%s", srctypestr) == 1 && srctypestr[0]) {
        srctype = mcx_keylookup(srctypestr, srctypeid);

        if (srctype == -1) {
            MMC_ERROR(-6, "the specified source type is not supported");
        }

        if (srctype >= 0) {
            comm = fgets(comment, MAX_PATH_LENGTH, in);
            cfg->srctype = srctype;

            if (in == stdin) {
                MMC_FPRINTF(stdout, ">> %d\nPlease specify the source parameters set 1 (4 floating-points):\n\t", cfg->srctype);
            }

            MMC_ASSERT(fscanf(in, "%f %f %f %f", &(cfg->srcparam1.x), &(cfg->srcparam1.y), &(cfg->srcparam1.z), &(cfg->srcparam1.w)) == 4);
            comm = fgets(comment, MAX_PATH_LENGTH, in);

            if (in == stdin)
                MMC_FPRINTF(stdout, ">> %f %f %f %f\nPlease specify the source parameters set 2 (4 floating-points):\n\t",
                            cfg->srcparam1.x, cfg->srcparam1.y, cfg->srcparam1.z, cfg->srcparam1.w);

            if (fscanf(in, "%f %f %f %f", &(cfg->srcparam2.x), &(cfg->srcparam2.y), &(cfg->srcparam2.z), &(cfg->srcparam2.w)) == 4) {
                comm = fgets(comment, MAX_PATH_LENGTH, in);

                if (in == stdin) {
                    MMC_FPRINTF(stdout, ">> %f %f %f %f\n", cfg->srcparam2.x, cfg->srcparam2.y, cfg->srcparam2.z, cfg->srcparam2.w);
                }

                if (cfg->srctype == stPattern && cfg->srcparam1.w * cfg->srcparam2.w > 0) {
                    char srcpatternfile[MAX_PATH_LENGTH];
                    FILE* fp;

                    if (in == stdin) {
                        MMC_FPRINTF(stdout, "Please specify the number of source patterns and source pattern file name:\n\t");
                    }

                    if (cfg->srcpattern) {
                        free(cfg->srcpattern);
                    }

                    // in case only one parameter is provided, use the default srcnum value (1)
                    MMC_ASSERT(fscanf(in, "%s %d", srcpatternfile, &(cfg->srcnum)) >= 1);

                    if (cfg->srcnum < 1) {
                        MMC_ERROR(-6, "the number of patterns cannot be smaller than 1");
                    }

                    cfg->srcpattern = (float*)calloc((cfg->srcparam1.w * cfg->srcparam2.w * cfg->srcnum), sizeof(float));
                    comm = fgets(comment, MAX_PATH_LENGTH, in);
                    fp = fopen(srcpatternfile, "rb");

                    if (fp == NULL) {
                        MMC_ERROR(-6, "source pattern file can not be opened");
                    }

                    MMC_ASSERT(fread(cfg->srcpattern, cfg->srcparam1.w * cfg->srcparam2.w * cfg->srcnum, sizeof(float), fp) == sizeof(float));
                    fclose(fp);
                }
            }

            if (cfg->detnum == 1 && cfg->detpos[0].w == 0.0) {
                // only one detector and its radius is 0, indicates that we are using a wide-field detector
                if (in == stdin) {
                    MMC_FPRINTF(stdout, ">> \nPlease specify the detector parameters set 1 (4 floating-points):\n\t");
                }

                MMC_ASSERT(fscanf(in, "%f %f %f %f", &(cfg->detparam1.x), &(cfg->detparam1.y), &(cfg->detparam1.z), &(cfg->detparam1.w)) == 4);
                comm = fgets(comment, MAX_PATH_LENGTH, in);

                if (in == stdin)
                    MMC_FPRINTF(stdout, ">> %f %f %f %f\nPlease specify the detector parameters set 2 (4 floating-points):\n\t",
                                cfg->detparam1.x, cfg->detparam1.y, cfg->detparam1.z, cfg->detparam1.w);

                MMC_ASSERT(fscanf(in, "%f %f %f %f", &(cfg->detparam2.x), &(cfg->detparam2.y), &(cfg->detparam2.z), &(cfg->detparam2.w)) == 4);
                comm = fgets(comment, MAX_PATH_LENGTH, in);

                if (in == stdin) {
                    MMC_FPRINTF(stdout, ">> %f %f %f %f\n", cfg->detparam2.x, cfg->detparam2.y, cfg->detparam2.z, cfg->detparam2.w);
                }

                // only load detection pattern under replay mode
                if (cfg->seed == SEED_FROM_FILE && (cfg->outputtype == otWL || cfg->outputtype == otWP) && cfg->detparam1.w * cfg->detparam2.w > 0) {
                    if (in == stdin) {
                        MMC_FPRINTF(stdout, "Please specify the detector pattern file name:\n\t");
                    }

                    char detpatternfile[MAX_PATH_LENGTH];
                    FILE* fp;

                    if (cfg->detpattern) {
                        free(cfg->detpattern);
                    }

                    cfg->detpattern = (float*)calloc((cfg->detparam1.w * cfg->detparam2.w), sizeof(float));
                    MMC_ASSERT(fscanf(in, "%s", detpatternfile) == 1);
                    comm = fgets(comment, MAX_PATH_LENGTH, in);
                    fp = fopen(detpatternfile, "rb");

                    if (fp == NULL) {
                        MMC_ERROR(-6, "detector pattern file can not be opened");
                    }

                    MMC_ASSERT(fread(cfg->detpattern, cfg->detparam1.w * cfg->detparam2.w, sizeof(float), fp) == sizeof(float));
                    fclose(fp);
                }
            }
        } else {
            return;
        }
    } else {
        return;
    }
}

/**
 * @brief Save simulation settings to an inp file
 *
 * @param[in] out: handle to the output file
 * @param[in] cfg: simulation configuration
 */

void mcx_saveconfig(FILE* out, mcconfig* cfg) {
    int i;

    MMC_FPRINTF(out, "%zu\n", (cfg->nphoton) );
    MMC_FPRINTF(out, "%d\n", (cfg->seed) );
    MMC_FPRINTF(out, "%f %f %f\n", (cfg->srcpos.x), (cfg->srcpos.y), (cfg->srcpos.z) );
    MMC_FPRINTF(out, "%f %f %f\n", (cfg->srcdir.x), (cfg->srcdir.y), (cfg->srcdir.z) );
    MMC_FPRINTF(out, "%f %f %f\n", (cfg->tstart), (cfg->tend), (cfg->tstep) );
    MMC_FPRINTF(out, "%f %d %d %d\n", (cfg->steps.x), (cfg->dim.x), (cfg->crop0.x), (cfg->crop1.x));
    MMC_FPRINTF(out, "%f %d %d %d\n", (cfg->steps.y), (cfg->dim.y), (cfg->crop0.y), (cfg->crop1.y));
    MMC_FPRINTF(out, "%f %d %d %d\n", (cfg->steps.z), (cfg->dim.z), (cfg->crop0.z), (cfg->crop1.z));
    MMC_FPRINTF(out, "%d", (cfg->medianum));

    for (i = 0; i < cfg->medianum; i++) {
        MMC_FPRINTF(out, "%f %f %f %f\n", (cfg->prop[i].mus), (cfg->prop[i].g), (cfg->prop[i].mua), (cfg->prop[i].n));
    }

    MMC_FPRINTF(out, "%d", (cfg->detnum));

    for (i = 0; i < cfg->detnum; i++) {
        MMC_FPRINTF(out, "%f %f %f %f\n", (cfg->detpos[i].x), (cfg->detpos[i].y), (cfg->detpos[i].z), (cfg->detpos[i].w));
    }
}


/**
 * @brief Save simulation settings to an inp file
 *
 * @param[in] out: handle to the output file
 * @param[in] cfg: simulation configuration
 */

void mcx_savejdata(char* filename, mcconfig* cfg) {
    cJSON* root = NULL, *obj = NULL, *sub = NULL, *tmp = NULL;
    char* jsonstr = NULL;
    int i;
    root = cJSON_CreateObject();

    /* the "Session" section */
    cJSON_AddItemToObject(root, "Session", obj = cJSON_CreateObject());
    cJSON_AddStringToObject(obj, "ID", cfg->session);
    cJSON_AddNumberToObject(obj, "Photons", cfg->nphoton);
    cJSON_AddNumberToObject(obj, "RNGSeed", (uint)cfg->seed);

    if (cfg->isreflect > 1) {
        cJSON_AddNumberToObject(obj, "DoMismatch", cfg->isreflect);
    } else {
        cJSON_AddBoolToObject(obj, "DoMismatch", cfg->isreflect);
    }

    cJSON_AddBoolToObject(obj, "DoSaveVolume", cfg->issave2pt);

    if (cfg->isreflect > 1) {
        cJSON_AddNumberToObject(obj, "DoNormalize", cfg->isnormalized);
    } else {
        cJSON_AddBoolToObject(obj, "DoNormalize", cfg->isnormalized);
    }

    cJSON_AddBoolToObject(obj, "DoPartialPath", cfg->issavedet);

    if (cfg->issaveref) {
        cJSON_AddNumberToObject(obj, "DoSaveRef", cfg->issaveref);
    } else {
        cJSON_AddBoolToObject(obj, "DoSaveRef", cfg->issaveref);
    }

    cJSON_AddBoolToObject(obj, "DoSaveExit", cfg->issaveexit);
    cJSON_AddBoolToObject(obj, "DoSaveSeed", cfg->issaveseed);
    cJSON_AddBoolToObject(obj, "DoAutoThread", cfg->autopilot);
    cJSON_AddBoolToObject(obj, "DoDCS", cfg->ismomentum);
    cJSON_AddBoolToObject(obj, "DoSpecular", cfg->isspecular);

    if (cfg->rootpath[0] != '\0') {
        cJSON_AddStringToObject(obj, "RootPath", cfg->rootpath);
    }

    cJSON_AddNumberToObject(obj, "DebugFlag", cfg->debuglevel);
    cJSON_AddNumberToObject(obj, "BasisOrder", cfg->basisorder);

    {
        char cflag[2] = {'\0', '\0'};
        cflag[0] = raytracing[(int)cfg->method];
        cJSON_AddStringToObject(obj, "RayTracer", cflag);
    }

    if (cfg->outputformat >= 0) {
        cJSON_AddStringToObject(obj, "OutputFormat", outputformat[(int)cfg->outputformat]);
    }

    if (cfg->outputtype >= 0) {
        char outputtypestr[2] = {'\0'};
        outputtypestr[0] = outputtype[(int)cfg->outputtype];
        cJSON_AddStringToObject(obj, "OutputType", outputtypestr);
    }

    /* the "Forward" section */
    cJSON_AddItemToObject(root, "Forward", obj = cJSON_CreateObject());
    cJSON_AddNumberToObject(obj, "T0", cfg->tstart);
    cJSON_AddNumberToObject(obj, "T1", cfg->tend);
    cJSON_AddNumberToObject(obj, "Dt", cfg->tstep);

    if (cfg->nout > 1.f) {
        cJSON_AddNumberToObject(obj, "N0", cfg->nout);
    }

    /* the "Domain" section */
    cJSON_AddItemToObject(root, "Domain", obj = cJSON_CreateObject());
    cJSON_AddNumberToObject(obj, "LengthUnit", cfg->unitinmm);
    cJSON_AddItemToObject(obj, "Media", sub = cJSON_CreateArray());

    for (i = 0; i < cfg->medianum; i++) {
        cJSON_AddItemToArray(sub, tmp = cJSON_CreateObject());
        cJSON_AddNumberToObject(tmp, "mua", cfg->prop[i].mua / cfg->unitinmm);
        cJSON_AddNumberToObject(tmp, "mus", cfg->prop[i].mus / cfg->unitinmm);
        cJSON_AddNumberToObject(tmp, "g",   cfg->prop[i].g);
        cJSON_AddNumberToObject(tmp, "n",   cfg->prop[i].n);
    }

    cJSON_AddItemToObject(obj, "Dim", cJSON_CreateIntArray((int*) & (cfg->dim.x), 3));

    /* the "Optode" section */
    cJSON_AddItemToObject(root, "Optode", obj = cJSON_CreateObject());
    cJSON_AddItemToObject(obj, "Source", sub = cJSON_CreateObject());

    if (cfg->srctype >= 0) {
        cJSON_AddStringToObject(sub, "Type", srctypeid[(int)cfg->srctype]);
    }

    cJSON_AddItemToObject(sub, "Pos", cJSON_CreateFloatArray(&(cfg->srcpos.x), 3));
    cJSON_AddItemToObject(sub, "Dir", cJSON_CreateFloatArray(&(cfg->srcdir.x), 4));
    cJSON_AddItemToObject(sub, "Param1", cJSON_CreateFloatArray(&(cfg->srcparam1.x), 4));
    cJSON_AddItemToObject(sub, "Param2", cJSON_CreateFloatArray(&(cfg->srcparam2.x), 4));
    cJSON_AddNumberToObject(sub, "SrcNum", cfg->srcnum);

    cJSON_AddItemToObject(obj, "Detector", sub = cJSON_CreateArray());

    for (i = 0; i < cfg->detnum; i++) {
        cJSON_AddItemToArray(sub, tmp = cJSON_CreateObject());
        cJSON_AddItemToObject(tmp, "Pos", cJSON_CreateFloatArray(&(cfg->detpos[i].x), 3));
        cJSON_AddNumberToObject(tmp, "R", cfg->detpos[i].w);
    }

    /* save "Shapes" constructs, containing InitElem, MeshNode, MeshElem, and MeshROI */
    cJSON_AddItemToObject(root, "Shapes", obj = cJSON_CreateObject());

    if (cfg->meshtag[0] && cfg->nodenum == 0) {
        cJSON_AddStringToObject(obj, "MeshID", cfg->meshtag);
    } else if (cfg->nodenum && cfg->elemnum) {
        uint dims[2] = {0};
        cJSON_AddItemToObject(obj, "MeshNode", sub = cJSON_CreateObject());

        dims[0] = cfg->nodenum;
        dims[1] = 3;

        if (mcx_jdataencode((void*)cfg->node, 2, dims, "single", 4, cfg->zipid, sub, 0, 0, cfg)) {
            MMC_ERROR(-10, "saving node data to JSON failed");
        }

        cJSON_AddItemToObject(obj, "MeshElem", sub = cJSON_CreateObject());

        dims[0] = cfg->elemnum;
        dims[1] = cfg->elemlen + 1;

        if (mcx_jdataencode((void*)cfg->elem, 2, dims, "int32", 4, cfg->zipid, sub, 0, 0, cfg)) {
            MMC_ERROR(-10, "saving elem data to JSON failed");
        }

        if (cfg->roidata) {
            cJSON_AddItemToObject(obj, "MeshROI", sub = cJSON_CreateObject());

            dims[0] = (cfg->roitype == rtNode ? cfg->nodenum : cfg->elemnum);
            dims[1] = (cfg->roitype == rtNode ? 1 : (cfg->roitype == rtEdge ? 6 : 4));

            if (mcx_jdataencode((void*)cfg->roidata, 2, dims, "single", 4, cfg->zipid, sub, 0, 0, cfg)) {
                MMC_ERROR(-10, "saving mesh ROI data to JSON failed");
            }
        }
    } else {
        MMC_ERROR(-10, "incomplete mesh data");
    }

    cJSON_AddNumberToObject(obj, "InitElem", cfg->e0);

    /* now save JSON to file */
    jsonstr = cJSON_Print(root);

    if (jsonstr == NULL) {
        MMC_ERROR(-1, "error when converting to JSON");
    }

    if (!strcmp(filename, "-")) {
        fprintf(cfg->flog, "%s\n", jsonstr);
    } else {
        FILE* fp = fopen(filename, "wt");

        if (fp == NULL) {
            MMC_ERROR(-1, "error opening file to write");
        }

        fprintf(fp, "%s\n", jsonstr);
        fclose(fp);
    }

    if (jsonstr) {
        free(jsonstr);
    }

    if (root) {
        cJSON_Delete(root);
    }
}

#endif

/**
 * @brief Convert a column-major (MATLAB/FORTRAN) array to a row-major (C/C++) array
 *
 * @param[in,out] vol: a 3D array (wrapped in 1D) to be converted
 * @param[in] dim: the dimensions of the 3D array
 */

void  mcx_convertcol2row(unsigned int** vol, uint3* dim) {
    uint x, y, z;
    unsigned int dimxy, dimyz;
    unsigned int* newvol = NULL;

    if (*vol == NULL || dim->x == 0 || dim->y == 0 || dim->z == 0) {
        return;
    }

    newvol = (unsigned int*)malloc(sizeof(unsigned int) * dim->x * dim->y * dim->z);
    dimxy = dim->x * dim->y;
    dimyz = dim->y * dim->z;

    for (z = 0; z < dim->z; z++)
        for (y = 0; y < dim->y; y++)
            for (x = 0; x < dim->x; x++) {
                newvol[x * dimyz + y * dim->z + z] = (*vol)[z * dimxy + y * dim->x + x];
            }

    free(*vol);
    *vol = newvol;
}

/**
 * @brief Convert a column-major (MATLAB/FORTRAN) array to a row-major (C/C++) array
 *
 * @param[in,out] vol: a 3D array (wrapped in 1D) to be converted
 * @param[in] dim: the dimensions of the 3D array
 */

void  mcx_convertcol2row4d(unsigned int** vol, uint4* dim) {
    uint x, y, z, w;
    unsigned int dimxyz, dimyzw, dimxy, dimzw;
    unsigned int* newvol = NULL;

    if (*vol == NULL || dim->x == 0 || dim->y == 0 || dim->z == 0 || dim->w == 0) {
        return;
    }

    newvol = (unsigned int*)malloc(sizeof(unsigned int) * dim->x * dim->y * dim->z * dim->w);
    dimxyz = dim->x * dim->y * dim->z;
    dimyzw = dim->y * dim->z * dim->w;
    dimxy = dim->x * dim->y;
    dimzw = dim->z * dim->w;

    for (w = 0; w < dim->w; w++)
        for (z = 0; z < dim->z; z++)
            for (y = 0; y < dim->y; y++)
                for (x = 0; x < dim->x; x++) {
                    newvol[x * dimyzw + y * dimzw + z * dim->w + w] = (*vol)[w * dimxyz + z * dimxy + y * dim->x + x];
                }

    free(*vol);
    *vol = newvol;
}

#ifndef MCX_CONTAINER


/**
 * @brief Decode an ND array from JSON/JData construct and output to a volumetric array
 *
 * The JData specification defines a portable way to encode and share volumetric
 * ND arrays and other complex data structures, such as trees, graphs and tables.
 * This function is capable of importing any ND numerical arrays in the JData
 * construct in to a generic array, permitting data decompression and base64 decoding.
 *
 * @param[in] vol: a pointer that points to the ND array buffer
 * @param[in] ndim: the number of dimensions
 * @param[in] dims: an integer pointer that points to the dimensional vector
 * @param[in] type: a string of JData data types, such as "uint8" "float32", "int32" etc
 * @param[in] byte: number of byte per voxel
 * @param[in] zipid: zip method: 0:zlib,1:gzip,2:base64,3:lzma,4:lzip,5:lz4,6:lz4hc
 * @param[in] obj: a pre-created cJSON object to store the output JData fields
 */

int  mcx_jdatadecode(void** vol, int* ndim, uint* dims, int maxdim, char** type, cJSON* obj, mcconfig* cfg) {
    int ret = 0;
    cJSON* ztype = NULL;
    cJSON* vsize = cJSON_GetObjectItem(obj, "_ArraySize_");
    cJSON* vtype = cJSON_GetObjectItem(obj, "_ArrayType_");
    cJSON* vdata = cJSON_GetObjectItem(obj, "_ArrayData_");

    if (!vdata) {
        ztype = cJSON_GetObjectItem(obj, "_ArrayZipType_");
        vdata = cJSON_GetObjectItem(obj, "_ArrayZipData_");
    }

    if (!flagset['K'] && vtype) {
        *type = vtype->valuestring;

        if (strstr(*type, "int8")) {
            cfg->mediabyte = 1;
        } else if (strstr(*type, "int16")) {
            cfg->mediabyte = 2;
        } else if (strstr(*type, "double") || strstr(*type, "int64")) {
            MMC_ERROR(-1, "8-byte volume array is not supported");
        } else {
            cfg->mediabyte = 4;
        }
    }

    if (vdata) {
        if (vsize) {
            cJSON* tmp = vsize->child;
            *ndim = cJSON_GetArraySize(vsize);

            for (int i = 0; i < MIN(maxdim, *ndim); i++) {
                dims[i] = tmp->valueint;
                tmp = tmp->next;
            }
        }

        if (ztype) {
            size_t len, newlen;
            int status = 0;
            char* buf = NULL;
            int zipid = mcx_keylookup((char*)(ztype->valuestring), zipformat);
            ret = zmat_decode(strlen(vdata->valuestring), (uchar*)vdata->valuestring, &len, (uchar**)&buf, zmBase64, &status);

            if (!ret && vsize) {
                if (*vol) {
                    free(*vol);
                }

                ret = zmat_decode(len, (uchar*)buf, &newlen, (uchar**)(vol), zipid, &status);
            }

            if (buf) {
                free(buf);
            }

            cfg->isrowmajor = 1;
        } else {
            MMC_ERROR(-1, "Only compressed JData array constructs are supported");
        }
    } else {
        MMC_ERROR(-1, "No _ArrayZipData_ field is found");
    }

    return ret;
}

/**
 * @brief Export an ND volumetric image to JSON/JData encoded construct
 *
 * The JData specification defines a portable way to encode and share volumetric
 * ND arrays and other complex data structures, such as trees, graphs and tables.
 * This function is capable of exporting any ND numerical arrays into a JData
 * construct, permitting data compression and base64 encoding.
 *
 * @param[in] vol: a pointer that points to the ND array buffer
 * @param[in] ndim: the number of dimensions
 * @param[in] dims: an integer pointer that points to the dimensional vector
 * @param[in] type: a string of JData data types, such as "uint8" "float32", "int32" etc
 * @param[in] byte: number of byte per voxel
 * @param[in] zipid: zip method: 0:zlib,1:gzip,2:base64,3:lzma,4:lzip,5:lz4,6:lz4hc
 * @param[in] obj: a pre-created cJSON or UBJ object to store the output JData fields
 * @param[in] isubj: 1 if obj is a binary JSON (UBJ) object, 0 if obj is a cJSON object
 * @param[in] cfg: mcx config struct
 */

int  mcx_jdataencode(void* vol, int ndim, uint* dims, char* type, int byte, int zipid, void* obj, int isubj, int iscol, mcconfig* cfg) {
    uint datalen = 1;
    size_t compressedbytes, totalbytes;
    uchar* compressed = NULL, *buf = NULL;
    int ret = 0, status = 0;

    for (int i = 0; i < ndim; i++) {
        datalen *= dims[i];
    }

    totalbytes = datalen * byte;

    if (!cfg->isdumpjson) {
        MMC_FPRINTF(cfg->flog, "compressing data [%s] ...", zipformat[zipid]);
    }

    /*compress data using zlib*/
    if (zipid != zmBase64) {
        ret = zmat_encode(totalbytes, (uchar*)vol, &compressedbytes, (uchar**)&compressed, zipid, &status);
    } else {
        compressed = (uchar*)vol;
        compressedbytes = totalbytes;
    }

    if (!ret) {
        if (!cfg->isdumpjson) {
            MMC_FPRINTF(cfg->flog, "compression ratio: %.1f%%\t", compressedbytes * 100.f / totalbytes);
        }

        if (isubj) {
            ubjw_context_t* item = (ubjw_context_t*)obj;
            UBJ_WRITE_KEY(item, "_ArrayType_", string, type);
            ubjw_write_key(item, "_ArraySize_");
            UBJ_WRITE_ARRAY(item, uint32, ndim, dims);

            if (iscol) {
                UBJ_WRITE_KEY(item, "_ArrayOrder_", string, "c");
            }

            UBJ_WRITE_KEY(item, "_ArrayZipType_", string, zipformat[zipid]);
            UBJ_WRITE_KEY(item, "_ArrayZipSize_", uint32, datalen);
            ubjw_write_key(item, "_ArrayZipData_");
            ubjw_write_buffer(item, compressed, UBJ_UINT8, compressedbytes);
        } else {
            totalbytes = 0;
            /*encode data using base64*/
            ret = zmat_encode(compressedbytes, compressed, &totalbytes, (uchar**)&buf, zmBase64, &status);

            if (!cfg->isdumpjson) {
                MMC_FPRINTF(cfg->flog, "after encoding: %.1f%%\n", totalbytes * 100.f / (datalen * byte));
            }

            if (!ret) {
                cJSON_AddStringToObject((cJSON*)obj, "_ArrayType_", type);
                cJSON_AddItemToObject((cJSON*)obj,   "_ArraySize_", cJSON_CreateIntArray((int*)dims, ndim));

                if (iscol) {
                    cJSON_AddStringToObject((cJSON*)obj, "_ArrayOrder_", "c");
                }

                cJSON_AddStringToObject((cJSON*)obj, "_ArrayZipType_", zipformat[zipid]);
                cJSON_AddNumberToObject((cJSON*)obj, "_ArrayZipSize_", datalen);
                cJSON_AddStringToObject((cJSON*)obj, "_ArrayZipData_", (char*)buf);
            }
        }
    }

    if (compressed && zipid != zmBase64) {
        free(compressed);
    }

    if (buf) {
        free(buf);
    }

    return ret;
}

#endif

/**
 * @brief Parse the debug flag in the letter format
 *
 * The debug flag following the -D can be either a string format, or numerical format.
 * This function converts the string debug flags into number format
 *
 * @param[in] debugopt: string following the -D parameter
 * @param[out] debugflag: the numerical format of the debug flag
 */

int mcx_parsedebugopt(char* debugopt, const char* debugflag) {
    char* c = debugopt, *p;
    int debuglevel = 0;

    while (*c) {
        p = strchr((char*)debugflag, ((*c <= 'z' && *c >= 'a') ? *c - 'a' + 'A' : *c) );

        if (p != NULL) {
            debuglevel |= (1 << (p - debugflag));
        }

        c++;
    }

    return debuglevel;
}


/**
 * @brief Flush command line output
 *
 * @param[in] out: the stream to be flushed
 */

void mcx_fflush(FILE* out) {
#if defined(MCX_CONTAINER) && (defined(MATLAB_MEX_FILE) || defined(OCTAVE_API_VERSION_NUMBER))
    mcx_matlab_flush();
#elif defined(PYBIND11_VERSION_MAJOR)
    mcx_python_flush();
#else
    fflush(out);
#endif
}

/**
 * @brief Print a progress bar
 *
 * When -D P is specified, this function prints and update a progress bar.
 *
 * @param[in] n: the number of completed photons
 * @param[in] cfg: simulation configuration
 */

void mcx_progressbar(float percent) {
    unsigned int percentage, j, colwidth = 79;
    static unsigned int oldmarker = 0xFFFFFFFF;

#ifndef MCX_CONTAINER
#ifdef TIOCGWINSZ
    struct winsize ttys = {0, 0, 0, 0};
    ioctl(0, TIOCGWINSZ, &ttys);
    colwidth = ttys.ws_col;
#elif defined(NCURSES_CONST)
    colwidth = tgetnum("co");
#endif

    if (colwidth == 0) {
        colwidth = 79;
    }

#endif

    percent = MIN(percent, 1.f);

    percentage = percent * (colwidth - 18);

    if (percentage != oldmarker) {
        if (percent != -0.f)
            for (j = 0; j < colwidth; j++) {
                MMC_FPRINTF(stdout, "\b");
            }

        oldmarker = percentage;
        MMC_FPRINTF(stdout, S_YELLOW"Progress: [");

        for (j = 0; j < percentage; j++) {
            MMC_FPRINTF(stdout, "=");
        }

        MMC_FPRINTF(stdout, (percentage < colwidth - 18) ? ">" : "=");

        for (j = percentage; j < colwidth - 18; j++) {
            MMC_FPRINTF(stdout, " ");
        }

        MMC_FPRINTF(stdout, "] %3d%%" S_RESET, (int)(percent * 100));

        mcx_fflush(stdout);
    }
}

#ifndef MCX_CONTAINER

/**
 * @brief Function to read a single parameter value followed by a command line option
 *
 * This function reads different types of parameter values following a command line option.
 *
 * @param[in] argc: the number of total command line parameters
 * @param[in] argv: the pointer to all command line options
 * @param[in] id: which parameter to be parsed
 * @param[out] output: the pointer to which the parsed value to be written
 * @param[in] type: the type of data support char, int, float, string, bytenumlist, floatlist
 */

int mcx_readarg(int argc, char* argv[], int id, void* output, const char* type) {
    /*
        when a binary option is given without a following number (0~1),
        we assume it is 1
    */
    if (strcmp(type, "bool") == 0 && (id >= argc - 1 || (argv[id + 1][0] < '0' || argv[id + 1][0] > '9'))) {
        *((char*)output) = 1;
        return id;
    }

    if (id < argc - 1) {
        if (strcmp(type, "bool") == 0) {
            *((char*)output) = atoi(argv[id + 1]);
        } else if (strcmp(type, "char") == 0) {
            *((char*)output) = argv[id + 1][0];
        } else if (strcmp(type, "int") == 0) {
            *((int*)output) = atoi(argv[id + 1]);
        } else if (strcmp(type, "float") == 0) {
            *((float*)output) = atof(argv[id + 1]);
        } else if (strcmp(type, "string") == 0) {
            strcpy((char*)output, argv[id + 1]);
        } else if (strcmp(type, "bytenumlist") == 0) {
            char* nexttok, *numlist = (char*)output;
            int len = 0, i;
            nexttok = strtok(argv[id + 1], " ,;");

            while (nexttok) {
                numlist[len++] = (char)(atoi(nexttok)); /*device id<256*/

                for (i = 0; i < len - 1; i++) /* remove duplicaetd ids */
                    if (numlist[i] == numlist[len - 1]) {
                        numlist[--len] = '\0';
                        break;
                    }

                nexttok = strtok(NULL, " ,;");
                /*if(len>=MAX_DEVICE) break;*/
            }
        } else if (strcmp(type, "floatlist") == 0) {
            char* nexttok;
            float* numlist = (float*)output;
            int len = 0;
            nexttok = strtok(argv[id + 1], " ,;");

            while (nexttok) {
                numlist[len++] = atof(nexttok); /*device id<256*/
                nexttok = strtok(NULL, " ,;");
            }
        }
    } else {
        MMC_ERROR(-1, "incomplete input");
    }

    return id + 1;
}

#endif

/**
 * @brief Test if a long command line option is supported
 *
 * This function returns 1 if a long option is found, and 0 otherwise
 *
 * @param[in] opt: the long command line option string
 */

int mcx_remap(char* opt) {
    int i = 0;

    while (shortopt[i] != '\0') {
        if (strcmp(opt, fullopt[i]) == 0) {
            opt[1] = shortopt[i];

            if (shortopt[i] != '-') {
                opt[2] = '\0';
            }

            return 0;
        }

        i++;
    }

    return 1;
}

/**
 * @brief Look up a single character in a string
 *
 * @param[in] key: character to be looked up
 * @param[out] index: the dictionary string where the char is searched
 * @return if found, return 0; otherwise, return 1
 */

int mcx_lookupindex(char* key, const char* index) {
    int i = 0;

    while (index[i] != '\0') {
        if (tolower(*key) == index[i]) {
            *key = i;
            return 0;
        }

        i++;
    }

    return 1;
}

/**
 * @brief Look up a string in a string list and return the index
 *
 * @param[in] key: string to be looked up
 * @param[out] table: the dictionary where the string is searched
 * @return if found, return the index of the string in the dictionary, otherwise -1.
 */

int mcx_keylookup(char* origkey, const char* table[]) {
    int i = 0;
    char* key = malloc(strlen(origkey) + 1);
    memcpy(key, origkey, strlen(origkey) + 1);

    while (key[i]) {
        key[i] = tolower(key[i]);
        i++;
    }

    i = 0;

    while (table[i] && table[i][0] != '\0') {
        if (strcmp(key, table[i]) == 0) {
            free(key);
            return i;
        }

        i++;
    }

    free(key);
    return -1;
}

int mcx_isbinstr(const char* str) {
    int i, len = strlen(str);

    if (len == 0) {
        return 0;
    }

    for (i = 0; i < len; i++)
        if (str[i] != '0' && str[i] != '1') {
            return 0;
        }

    return 1;
}

/**
 * @brief Convert a row-major (C/C++) array to a column-major (MATLAB/FORTRAN) array
 *
 * @param[in,out] vol: a 3D array (wrapped in 1D) to be converted
 * @param[in] dim: the dimensions of the 3D array
 */

void  mcx_convertrow2col(float* vol, uint3* dim) {
    uint x, y, z;
    unsigned int dimxy, dimyz;
    float* newvol = NULL;

    if (vol == NULL || dim->x == 0 || dim->y == 0 || dim->z == 0) {
        return;
    }

    newvol = (float*)malloc(sizeof(float) * dim->x * dim->y * dim->z);
    dimxy = dim->x * dim->y;
    dimyz = dim->y * dim->z;

    for (x = 0; x < dim->x; x++)
        for (y = 0; y < dim->y; y++)
            for (z = 0; z < dim->z; z++) {
                newvol[z * dimxy + y * dim->x + x] = vol[x * dimyz + y * dim->z + z];
            }

    memcpy(vol, newvol, sizeof(float) * dim->x * dim->y * dim->z);
    free(newvol);
}

/**
 * @brief Validate all input fields, and warn incompatible inputs
 *
 * Perform self-checking and raise exceptions or warnings when input error is detected
 *
 * @param[in,out] cfg: the simulation configuration structure
 */

void mcx_validatecfg(mcconfig* cfg) {
    int i;

    if (cfg->nphoton <= 0) {
        MMC_ERROR(-2, "cfg.nphoton must be a positive number");
    }

    if (cfg->tstart > cfg->tend || cfg->tstep == 0.f) {
        MMC_ERROR(-2, "incorrect time gate settings or missing tstart/tend/tstep fields");
    }

    if (cfg->tstep > cfg->tend - cfg->tstart) {
        cfg->tstep = cfg->tend - cfg->tstart;
    }

    if (cfg->steps.x != cfg->steps.y || cfg->steps.y != cfg->steps.z) {
        MMC_ERROR(-2, "MMC dual-grid algorithm currently does not support anisotropic voxels");
    }

    if (fabs(cfg->srcdir.x * cfg->srcdir.x + cfg->srcdir.y * cfg->srcdir.y + cfg->srcdir.z * cfg->srcdir.z - 1.f) > 1e-4) {
        MMC_ERROR(-2, "field 'srcdir' must be a unitary vector (tolerance is 1e-4)");
    }

    if (cfg->tend <= cfg->tstart) {
        MMC_ERROR(-2, "field 'tend' must be greater than field 'tstart'");
    }

    cfg->maxgate = (int)((cfg->tend - cfg->tstart) / cfg->tstep + 0.5);
    cfg->tend = cfg->tstart + cfg->tstep * cfg->maxgate;

    if (cfg->srctype == stPattern && cfg->srcpattern == NULL) {
        MMC_ERROR(-2, "the 'srcpattern' field can not be empty when your 'srctype' is 'pattern'");
    }

    if (cfg->srcnum > 1 && cfg->seed == SEED_FROM_FILE) {
        MMC_ERROR(-2, "multiple source simulation is currently not supported under replay mode");
    }

    if (cfg->seed < 0 && cfg->seed != SEED_FROM_FILE) {
        cfg->seed = time(NULL);
    }

    if (cfg->compute != cbSSE && (cfg->method != rtBLBadouelGrid && cfg->method != rtBLBadouel)) {
        cfg->method = rtBLBadouel;
    }

#ifndef MMC_USE_SSE

    if (cfg->gpuid > MAX_DEVICE) {
        cfg->method = rtPlucker;
    }

#endif

    if (cfg->method == rtBLBadouelGrid) {
        cfg->basisorder = 0;
    }

    if (cfg->implicit && (int)(cfg->gpuid) >= 0) {
        MMC_ERROR(-2, "Implicit MMC is currently only supported in the CPU, please set -G -1 or cfg.gpuid=-1");
    }

    for (i = 0; i < MAX_DEVICE; i++)
        if (cfg->deviceid[i] == '0') {
            cfg->deviceid[i] = '\0';
        }
}

/**
 * @brief Initialize the replay data structure from detected photon data - in embedded mode (MATLAB/Python)
 *
 * @param[in,out] cfg: simulation configuration
 * @param[in] detps: detected photon data
 * @param[in] dimdetps: the dimension vector of the detected photon data
 * @param[in] seedbyte: the number of bytes per RNG seed
 */

void mcx_replayinit(mcconfig* cfg, float* detps, int dimdetps[2], int seedbyte) {
    int i, j, hasdetid = 0, offset;
    float plen;

    if (cfg->seed == SEED_FROM_FILE && detps == NULL) {
        MMC_ERROR(-6, "you give cfg.seed for replay, but did not specify cfg.detphotons.\nPlease define it as the detphoton output from the baseline simulation\n");
    }

    if (detps == NULL || cfg->seed != SEED_FROM_FILE) {
        return;
    }

    if (cfg->nphoton != dimdetps[1]) {
        MMC_ERROR(-6, "the column numbers of detphotons and seed do not match\n");
    }

    if (seedbyte == 0) {
        MMC_ERROR(-6, "the seed input is empty");
    }

    hasdetid = SAVE_DETID(cfg->savedetflag);
    offset = SAVE_NSCAT(cfg->savedetflag) * (cfg->medianum - 1);

    if (((!hasdetid) && cfg->detnum > 1) || !SAVE_PPATH(cfg->savedetflag)) {
        MMC_ERROR(-6, "please rerun the baseline simulation and save detector ID (D) and partial-path (P) using cfg.savedetflag='dp' ");
    }

    cfg->replayweight = (float*) malloc(cfg->nphoton * sizeof(float));
    cfg->replaytime = (float*) calloc(cfg->nphoton, sizeof(float));

    cfg->nphoton = 0;

    for (i = 0; i < dimdetps[1]; i++) {
        if (cfg->replaydet <= 0 || cfg->replaydet == (int) (detps[i * dimdetps[0]])) {
            if (i != cfg->nphoton) {
                memcpy((char*) (cfg->photonseed) + cfg->nphoton * seedbyte,
                       (char*) (cfg->photonseed) + i * seedbyte,
                       seedbyte);
            }

            cfg->replayweight[cfg->nphoton] = 1.f;
            cfg->replaytime[cfg->nphoton] = 0.f;

            for (j = hasdetid; j < cfg->medianum - 1 + hasdetid; j++) {
                plen = detps[i * dimdetps[0] + offset + j];
                cfg->replayweight[cfg->nphoton] *= expf(-cfg->prop[j - hasdetid + 1].mua * plen);
                plen *= cfg->unitinmm;
                cfg->replaytime[cfg->nphoton] += plen * R_C0 * cfg->prop[j - hasdetid + 1].n;
            }

            if (cfg->replaytime[cfg->nphoton] < cfg->tstart
                    || cfg->replaytime[cfg->nphoton] > cfg->tend) { /*need to consider -g*/
                continue;
            }

            cfg->nphoton++;
        }
    }

    cfg->replayweight = (float*) realloc(cfg->replayweight, cfg->nphoton * sizeof(float));
    cfg->replaytime = (float*) realloc(cfg->replaytime, cfg->nphoton * sizeof(float));
}

/**
 * @brief Validate all input fields, and warn incompatible inputs
 *
 * Perform self-checking and raise exceptions or warnings when input error is detected
 *
 * @param[in,out] cfg: the simulation configuration structure
 * @param[out] mesh: the mesh data structure
 */

void mmc_validate_config(mcconfig* cfg, float* detps, int dimdetps[2], int seedbyte) {
    if (cfg->nphoton <= 0) {
        MMC_ERROR(999, "cfg.nphoton must be a positive number");
    }

    if (cfg->tstart > cfg->tend || cfg->tstep == 0.f) {
        MMC_ERROR(999, "incorrect time gate settings or missing tstart/tend/tstep fields");
    }

    if (cfg->tstep > cfg->tend - cfg->tstart) {
        cfg->tstep = cfg->tend - cfg->tstart;
    }

    if (ABS(cfg->srcdir.x * cfg->srcdir.x + cfg->srcdir.y * cfg->srcdir.y + cfg->srcdir.z * cfg->srcdir.z - 1.f) > 1e-5) {
        MMC_ERROR(999, "field 'srcdir' must be a unitary vector");
    }

    if (cfg->tend <= cfg->tstart) {
        MMC_ERROR(999, "field 'tend' must be greater than field 'tstart'");
    }

    cfg->maxgate = (int)((cfg->tend - cfg->tstart) / cfg->tstep + 0.5);
    cfg->tend = cfg->tstart + cfg->tstep * cfg->maxgate;

    if (cfg->srctype == stPattern && cfg->srcpattern == NULL) {
        MMC_ERROR(999, "the 'srcpattern' field can not be empty when your 'srctype' is 'pattern'");
    }

    if (cfg->srcnum > 1 && cfg->seed == SEED_FROM_FILE) {
        MMC_ERROR(999, "multiple source simulation is currently not supported under replay mode");
    }

    cfg->his.unitinmm = cfg->unitinmm;

    if (cfg->steps.x != cfg->steps.y || cfg->steps.y != cfg->steps.z) {
        MMC_ERROR(999, "MMC dual-grid algorithm currently does not support anisotropic voxels");
    }

    if (cfg->issavedet && cfg->detnum == 0 && cfg->isextdet == 0) {
        cfg->issavedet = 0;
    }

    if (cfg->seed < 0 && cfg->seed != SEED_FROM_FILE) {
        cfg->seed = time(NULL);
    }

    if (cfg->issavedet == 0) {
        cfg->ismomentum = 0;
        cfg->issaveexit = 0;
    }

    cfg->his.detnum = cfg->detnum;
    cfg->his.colcount = (1 + (cfg->ismomentum > 0)) * cfg->his.maxmedia + (cfg->issaveexit > 0) * 6 + 1;
    mcx_replayinit(cfg, detps, dimdetps, seedbyte);
}

/**
 * @brief Preprocess configuration and set option dependency
 *
 * This function preprocess the user input and set dependent flags
 *
 * @param[in,out] cfg: simulation configuration
 */

void mcx_prep(mcconfig* cfg) {
    if (cfg->issavedet && cfg->detnum == 0 && cfg->isextdet == 0) {
        cfg->issavedet = 0;
    }

    if (cfg->issavedet == 0) {
        cfg->ismomentum = 0;
        cfg->issaveexit = 0;
    }

    cfg->savedetflag = 0x47;

    if (cfg->ismomentum) {
        cfg->savedetflag = SET_SAVE_MOM(cfg->savedetflag);
    }

    if (cfg->issaveexit) {
        cfg->savedetflag = SET_SAVE_PEXIT(cfg->savedetflag);
        cfg->savedetflag = SET_SAVE_VEXIT(cfg->savedetflag);
    }

}

#ifndef MCX_CONTAINER

/**
 * @brief Main function to read user command line options
 *
 * This function process user command line inputs and parse all short and long options.
 *
 * @param[in] argc: the number of total command line parameters
 * @param[in] argv: the pointer to all command line options
 * @param[in] cfg: simulation configuration
 */

void mcx_parsecmd(int argc, char* argv[], mcconfig* cfg) {
    int i = 1, isinteractive = 1, issavelog = 0;
    char filename[MAX_PATH_LENGTH] = {0}, *jsoninput = NULL;
    char logfile[MAX_PATH_LENGTH] = {0};
    float np = 0.f;

#if defined(_WIN32) && defined(USE_OS_TIMER) && !defined(MCX_CONTAINER)
    EnableVTMode();
#endif

    if (argc <= 1) {
        mcx_usage(argv[0], cfg);
        exit(0);
    }

    while (i < argc) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == '-') {
                if (mcx_remap(argv[i])) {
                    MMC_FPRINTF(cfg->flog, "option: %s\n", argv[i]);
                    MMC_ERROR(-2, "unsupported verbose option");
                }
            }

            if (argv[i][1] <= 'z' && argv[i][1] >= 'A') {
                flagset[(int)(argv[i][1])] = 1;
            }

            switch (argv[i][1]) {
                case 'h':
                    mcx_usage(argv[0], cfg);
                    exit(0);

                case 'i':
                    if (filename[0]) {
                        MMC_ERROR(-2, "you can not specify both interactive mode and config file");
                    }

                    isinteractive = 1;
                    break;

                case 'f':
                    isinteractive = 0;

                    if (i < argc - 1 && argv[i + 1][0] == '{') {
                        jsoninput = argv[i + 1];
                        i++;
                    } else if (i == argc - 1 || argv[i + 1][0] == '-') {
                        runcommand("", "", &jsoninput);
                        isinteractive = 2;
                        i += (i < argc - 1);
                    } else {
                        i = mcx_readarg(argc, argv, i, filename, "string");
                    }

                    break;

                case 'n':
                    i = mcx_readarg(argc, argv, i, &np, "float");
                    cfg->nphoton = (size_t)np;
                    break;

                case 't':
                    i = mcx_readarg(argc, argv, i, &(cfg->nthread), "int");
                    break;

                case 'T':
                    i = mcx_readarg(argc, argv, i, &(cfg->nblocksize), "int");
                    break;

                case 's':
                    i = mcx_readarg(argc, argv, i, cfg->session, "string");
                    break;

                case 'q':
                    i = mcx_readarg(argc, argv, i, &(cfg->issaveseed), "bool");
                    break;

                case 'g':
                    i = mcx_readarg(argc, argv, i, &(cfg->maxgate), "int");
                    break;

                case 'b':
                    i = mcx_readarg(argc, argv, i, &(cfg->isreflect), "bool");
                    break;

                case 'd':
                    i = mcx_readarg(argc, argv, i, &(cfg->issavedet), "bool");
                    break;

                case 'm':
                    i = mcx_readarg(argc, argv, i, &(cfg->mcmethod), "int");
                    break;

                case 'x':
                    i = mcx_readarg(argc, argv, i, &(cfg->issaveexit), "bool");

                    if (cfg->issaveexit) {
                        cfg->issavedet = 1;
                    }

                    break;

                case 'X':
                    i = mcx_readarg(argc, argv, i, &(cfg->issaveref), "char");

                    if (cfg->issaveref) {
                        cfg->issaveref = 1;
                    }

                    break;

                case 'Z':
                    if (i + 1 < argc && isalpha((int)(argv[i + 1][0])) ) {
                        cfg->zipid = mcx_keylookup(argv[++i], zipformat);
                    } else {
                        i = mcx_readarg(argc, argv, i, &(cfg->zipid), "int");
                    }

                    break;

                case 'C':
                    i = mcx_readarg(argc, argv, i, &(cfg->basisorder), "bool");
                    break;

                case 'V':
                    i = mcx_readarg(argc, argv, i, &(cfg->isspecular), "bool");
                    break;

                case 'v':
                    mcx_version(cfg);
                    break;

                case 'r':
                    i = mcx_readarg(argc, argv, i, &(cfg->respin), "int");
                    break;

                case 'S':
                    i = mcx_readarg(argc, argv, i, &(cfg->issave2pt), "bool");
                    break;

                case 'e':
                    i = mcx_readarg(argc, argv, i, &(cfg->minenergy), "float");
                    break;

                case 'U':
                    i = mcx_readarg(argc, argv, i, &(cfg->isnormalized), "bool");
                    break;

                case 'E':
                    if (i + 1 < argc && strstr(argv[i + 1], ".mch") != NULL) { /*give an mch file to initialize the seed*/
#if defined(MMC_LOGISTIC) || defined(MMC_SFMT)
                        MMC_ERROR(-1, "seeding file is not supported in this binary");
#else
                        i = mcx_readarg(argc, argv, i, cfg->seedfile, "string");
                        cfg->seed = SEED_FROM_FILE;
#endif
                    } else {
                        i = mcx_readarg(argc, argv, i, &(cfg->seed), "int");
                    }

                    break;

                case 'F':
                    if (i >= argc) {
                        MMC_ERROR(-1, "incomplete input");
                    }

                    if ((cfg->outputformat = mcx_keylookup(argv[++i], outputformat)) < 0) {
                        MMC_ERROR(-2, "the specified output data type is not recognized");
                    }

                    break;

                case 'O':
                    i = mcx_readarg(argc, argv, i, &(cfg->outputtype), "char");

                    if (mcx_lookupindex(&(cfg->outputtype), outputtype)) {
                        MMC_ERROR(-2, "the specified output data type is not recognized");
                    }

                    break;

                case 'M':
                    i = mcx_readarg(argc, argv, i, &(cfg->method), "char");

                    if (mcx_lookupindex(&(cfg->method), raytracing)) {
                        MMC_ERROR(-2, "the specified ray-tracing method is not recognized");
                    }

                    break;

                case 'R':
                    i = mcx_readarg(argc, argv, i, &(cfg->sradius), "float");
                    break;

                case 'P':
                    i = mcx_readarg(argc, argv, i, &(cfg->replaydet), "int");
                    break;

                case 'u':
                    i = mcx_readarg(argc, argv, i, &(cfg->unitinmm), "float");
                    break;

                case 'l':
                    issavelog = 1;
                    break;

                case 'L':
                    cfg->isgpuinfo = 2;
                    break;

                case 'I':
                    cfg->isgpuinfo = 1;
                    break;

                case 'J':
                    cfg->compileropt[strlen(cfg->compileropt)] = ' ';
                    i = mcx_readarg(argc, argv, i, cfg->compileropt + strlen(cfg->compileropt), "string");
                    break;

                case 'o':
                    i = mcx_readarg(argc, argv, i, &(cfg->optlevel), "int");
                    break;

                case 'D':
                    if (i + 1 < argc && isalpha((int)argv[i + 1][0]) ) {
                        cfg->debuglevel = mcx_parsedebugopt(argv[++i], debugflag);
                    } else {
                        i = mcx_readarg(argc, argv, i, &(cfg->debuglevel), "int");
                    }

                    break;

                case 'k':
                    i = mcx_readarg(argc, argv, i, &(cfg->voidtime), "int");
                    break;

                case 'H':
                    i = mcx_readarg(argc, argv, i, &(cfg->maxdetphoton), "int");
                    break;

                case 'A':
                    i = mcx_readarg(argc, argv, i, &(cfg->autopilot), "int");
                    break;

                case 'c':
                    if (i + 1 < argc && isalpha((int)argv[i + 1][0]) ) {
                        cfg->compute = mcx_keylookup(argv[++i], computebackend);
                    } else {
                        i = mcx_readarg(argc, argv, i, &(cfg->compute), "int");
                    }

                    break;

                case 'G':
                    if (mcx_isbinstr(argv[i + 1])) {
                        i = mcx_readarg(argc, argv, i, cfg->deviceid, "string");
                        break;
                    } else {
                        i = mcx_readarg(argc, argv, i, &(cfg->gpuid), "int");
                        memset(cfg->deviceid, '0', MAX_DEVICE);

                        if (cfg->gpuid > 0 && cfg->gpuid < MAX_DEVICE) {
                            cfg->deviceid[cfg->gpuid - 1] = '1';
                        }

                        break;
                    }

                case 'W':
                    i = mcx_readarg(argc, argv, i, cfg->workload, "floatlist");
                    break;

                case 'Q':
                    if (i + 1 < argc && isalpha((int)(argv[i + 1][0])) ) {
                        int idx = mcx_keylookup(argv[++i], benchname);

                        if (idx == -1) {
                            MMC_ERROR(-1, "Unsupported bechmark.");
                        }

                        isinteractive = 0;
                        jsoninput = (char*)benchjson[idx];
                    } else {
                        MMC_FPRINTF(cfg->flog, "Built-in benchmarks:\n");

                        for (i = 0; i < sizeof(benchname) / sizeof(char*) - 1; i++) {
                            MMC_FPRINTF(cfg->flog, "\t%s\n", benchname[i]);
                        }

                        exit(0);
                    }

                    break;

                case 'N':
                    if (i == argc - 1 || argv[i + 1][0] == '-' || argv[i + 1][0] == '{' || strchr(argv[i + 1], '=')) {
                        int j, doclen;
                        char* jbuf = NULL;

                        if (i < argc - 1 && argv[i + 1][0] == '{') {
                            char cmd[MAX_PATH_LENGTH] = "curl -s -X POST -H 'Content-Type: application/json' -d ";
                            sprintf(cmd + strlen(cmd), "'%s' \"https://neurojson.io:7777/mmc/_find\"", argv[i + 1]);
                            runcommand(cmd, "", &jbuf);
                        } else if (i < argc - 1 && strchr(argv[i + 1], '=')) {
                            char cmd[MAX_PATH_LENGTH] = "curl -s -X GET 'https://neurojson.io:7777/mmc/_all_docs?";
                            sprintf(cmd + strlen(cmd), "%s'", argv[i + 1]);
                            runcommand(cmd, "", &jbuf);
                        } else {
                            runcommand("curl -s -X POST -H 'Content-Type: application/json' -d '{\"selector\": {\"Session\": {\"$gt\": null}},\"fields\": [\"_id\"],\"limit\":50}' \"https://neurojson.io:7777/mmc/_find\"", "", &jbuf);
                        }

                        cJSON* root = cJSON_Parse(jbuf), *docs = cJSON_GetObjectItem(root, "docs"), *subitem, *tmp;

                        if (!docs) {
                            docs = cJSON_GetObjectItem(root, "rows");

                            if (!docs || !root) {
                                MMC_ERROR(-1, jbuf);
                            }
                        }

                        doclen = cJSON_GetArraySize(docs);
                        subitem = docs->child;
                        printf("Downloading %d simulations from NeuroJSON.io (https://neurojson.org/db/mmc)\n", doclen - 1);

                        for (j = 0; j < doclen; j++) {
                            char* docid = (cJSON_GetObjectItem(subitem, "_id") ? FIND_JSON_KEY("_id", "id", subitem, "", valuestring) : FIND_JSON_KEY("id", "id", subitem, "", valuestring));

                            if (docid && docid[0] != '_' && docid[0] != '.') {
                                printf("\t%s\n", docid);
                            }

                            subitem = subitem->next;
                        }

                        cJSON_Delete(root);
                        free(jbuf);
                        exit(0);
                    } else {
                        if (strstr(argv[i + 1], "http") == argv[i + 1]) {
                            runcommand("curl -s -X GET ", argv[i + 1], &jsoninput);
                        } else if (strchr(argv[i + 1], '/')) {
                            runcommand("curl -s -X GET https://neurojson.io:7777/", argv[i + 1], &jsoninput);
                        } else {
                            runcommand("curl -s -X GET https://neurojson.io:7777/mmc/", argv[i + 1], &jsoninput);
                        }

                        isinteractive = 2;
                    }

                    break;

                case '-':  /*additional verbose parameters*/
                    if (strcmp(argv[i] + 2, "momentum") == 0) {
                        i = mcx_readarg(argc, argv, i, &(cfg->ismomentum), "bool");

                        if (cfg->ismomentum) {
                            cfg->issavedet = 1;
                        }
                    } else if (strcmp(argv[i] + 2, "atomic") == 0) {
                        i = mcx_readarg(argc, argv, i, &(cfg->isatomic), "bool");
                    } else if (strcmp(argv[i] + 2, "root") == 0) {
                        i = mcx_readarg(argc, argv, i, cfg->rootpath, "string");
                    } else if (strcmp(argv[i] + 2, "dumpjson") == 0) {
                        cfg->jsonfile[0] = '-';

                        if (i + 1 >= argc) {
                            cfg->isdumpjson = 1;
                            i++;
                        } else if (i + 1 < argc && (isalpha((int)(argv[i + 1][0])) || argv[i + 1][0] == '-')) {
                            cfg->isdumpjson = 1;
                            memcpy(cfg->jsonfile, argv[i + 1], MIN(strlen(argv[i + 1]), MAX_PATH_LENGTH));
                            i++;
                        } else {
                            i = mcx_readarg(argc, argv, i, &(cfg->isdumpjson), "int");
                        }
                    } else if (strcmp(argv[i] + 2, "debugphoton") == 0) {
                        i = mcx_readarg(argc, argv, i, &(cfg->debugphoton), "int");
                    } else if (strcmp(argv[i] + 2, "maxjumpdebug") == 0) {
                        i = mcx_readarg(argc, argv, i, &(cfg->maxjumpdebug), "int");
                    } else if (strcmp(argv[i] + 2, "gridsize") == 0) {
                        i = mcx_readarg(argc, argv, i, &(cfg->steps.x), "float");
                        cfg->steps.y = cfg->steps.x;
                        cfg->steps.z = cfg->steps.x;
                    } else {
                        MMC_FPRINTF(cfg->flog, "unknown verbose option: --%s\n", argv[i] + 2);
                    }

                    break;

                default: {
                    MMC_FPRINTF(cfg->flog, "option: %s\n", argv[i]);
                    MMC_ERROR(-1, "unsupported command line option");
                }
            }
        }

        i++;
    }

    if (issavelog && cfg->session[0]) {
        sprintf(logfile, "%s.log", cfg->session);
        cfg->flog = fopen(logfile, "wt");

        if (cfg->flog == NULL) {
            cfg->flog = stdout;
            MMC_FPRINTF(cfg->flog, "unable to save to log file, will print from stdout\n");
        }
    }

    if (cfg->kernelfile[0] != '\0' && cfg->isgpuinfo != 2) {
        FILE* fp = fopen(cfg->kernelfile, "rb");
        int srclen;

        if (fp == NULL) {
            mcx_error(-10, "the specified OpenCL kernel file does not exist!", __FILE__, __LINE__);
        }

        fseek(fp, 0, SEEK_END);
        srclen = ftell(fp);

        if (cfg->clsource != (char*)mmc_core_cl) {
            free(cfg->clsource);
        }

        cfg->clsource = (char*)malloc(srclen + 1);
        fseek(fp, 0, SEEK_SET);
        MMC_ASSERT((fread(cfg->clsource, srclen, 1, fp) == 1));
        cfg->clsource[srclen] = '\0';
        fclose(fp);
    }

    if ((cfg->outputtype == otJacobian || cfg->outputtype == otWL || cfg->outputtype == otWP) && cfg->seed != SEED_FROM_FILE) {
        MMC_ERROR(-1, "Jacobian output is only valid in the reply mode. Please give an mch file after '-E'.");
    }

    if (cfg->isgpuinfo != 2) { /*print gpu info only*/
        if (isinteractive == 2 && jsoninput) {
            mcx_loadfromjson(jsoninput, cfg);
            free(jsoninput);
        } else if (isinteractive) {
            mcx_readconfig("", cfg);
        } else if (jsoninput) {
            mcx_loadfromjson(jsoninput, cfg);
        } else {
            mcx_readconfig(filename, cfg);
        }
    }

    if (cfg->isgpuinfo == 0) {
        mcx_validatecfg(cfg);
    }
}

void mcx_savedetphoton(float* ppath, void* seeds, int count, int doappend, mcconfig* cfg) {
    FILE* fp;
    char fhistory[MAX_FULL_PATH];

    if (cfg->outputformat == ofJNifti || cfg->outputformat == ofBJNifti) {
        mcx_savejdet(ppath, seeds, count, doappend, cfg);
        return;
    }

    if (cfg->rootpath[0]) {
        sprintf(fhistory, "%s%c%s.mch", cfg->rootpath, pathsep, cfg->session);
    } else {
        sprintf(fhistory, "%s.mch", cfg->session);
    }

    if (doappend) {
        fp = fopen(fhistory, "ab");
    } else {
        fp = fopen(fhistory, "wb");
    }

    if (fp == NULL) {
        mcx_error(-2, "can not save data to disk", __FILE__, __LINE__);
    }

    fwrite(&(cfg->his), sizeof(history), 1, fp);
    fwrite(ppath, sizeof(float), count * cfg->his.colcount, fp);
    fclose(fp);
}

#endif

/**
 * @brief Print MCX software version
 *
 * @param[in] cfg: simulation configuration
 */

void mcx_version(mcconfig* cfg) {
    MMC_ERROR(MMC_INFO, "MMC $Rev::      $" MMC_VERSION);
}

/**
 * @brief Print MCX output header
 *
 * @param[in] cfg: simulation configuration
 */

void mcx_printheader(mcconfig* cfg) {
    MMC_FPRINTF(cfg->flog, S_YELLOW "\
###############################################################################\n\
#                     Mesh-based Monte Carlo (MMC) - OpenCL                   #\n\
#          Copyright (c) 2010-2025 Qianqian Fang <q.fang at neu.edu>          #\n\
#" S_BLUE "              https://mcx.space/#mmc  &  https://neurojson.io                " S_YELLOW "#\n\
#                                                                             #\n\
#Computational Optics & Translational Imaging (COTI) Lab  [http://fanglab.org]#\n\
#   Department of Bioengineering, Northeastern University, Boston, MA, USA    #\n\
###############################################################################\n\
#    The MCX Project is funded by the NIH/NIGMS under grant R01-GM114365      #\n\
###############################################################################\n\
#  Open-source codes and reusable scientific data are essential for research, #\n\
# MCX proudly developed human-readable JSON-based data formats for easy reuse.#\n\
#                                                                             #\n\
#Please visit our free scientific data sharing portal at " S_BLUE "https://neurojson.io " S_YELLOW "#\n\
# and consider sharing your public datasets in standardized JSON/JData format #\n\
###############################################################################\n\
$Rev::      $" S_GREEN MMC_VERSION S_YELLOW "$Date::                       $ by $Author::             $\n\
###############################################################################\n"S_RESET);
}

/**
 * @brief Print MCX help information
 *
 * @param[in] exename: path and name of the mcx executable
 */

void mcx_usage(char* exename, mcconfig* cfg) {
    mcx_printheader(cfg);
    printf("\n\
usage: %s <param1> <param2> ...\n\
where possible parameters include (the first item in [] is the default value)\n\
\n"S_BOLD S_CYAN"\
== Required option ==\n"S_RESET"\
 -f config     (--input)       read an input file in the .json format,if config\n\
                               string starts with '{',it is parsed as an inline\n\
                               JSON input file; if -f is followed by nothing or\n\
                               a single '-', it reads input from stdin via pipe\n\
 -Q benchmark  (--bench)       run a built-in benchmark, -Q only to list\n\
 -N benchmark  (--net)         get benchmark from NeuroJSON.io, -N only to list\n\
                               benchmark can be dataset URL,or dbname/benchname\n\
                               requires 'curl', install from https://curl.se/\n\
\n"S_BOLD S_CYAN"\
== MC options ==\n"S_RESET"\
 -n [0.|float] (--photon)      total photon number, max allowed value is 2^32-1\n\
 -b [0|1]      (--reflect)     1 do reflection at int&ext boundaries, 0 no ref.\n\
 -U [1|0]      (--normalize)   1 to normalize the fluence to unitary,0 save raw\n\
 -m [0|1]      (--mc)          0 use MCX-styled MC method, 1 use MCML style MC\n\
 -C [1|0]      (--basisorder)  1 piece-wise-linear basis for fluence,0 constant\n\
 -u [1.|float] (--unitinmm)    define the mesh data length unit in mm\n\
 -E [1648335518|int|mch](--seed) set random-number-generator seed;\n\
                               if an mch file is followed, MMC \"replays\" \n\
                               the detected photons; the replay mode can be used\n\
                               to calculate the mua/mus Jacobian matrices\n\
 -P [0|int]    (--replaydet)   replay only the detected photons from a given \n\
                               detector (det ID starts from 1), use with -E \n\
 -M [%c|SG]    (--method)      choose ray-tracing algorithm (only use 1 letter)\n\
                               P - Plucker-coordinate ray-tracing algorithm\n\
                               H - Havel's SSE4 ray-tracing algorithm\n\
                               B - partial Badouel's method (used by TIM-OS)\n\
                               S - branch-less Badouel's method with SSE\n\
                               G - dual-grid MMC (DMMC) with voxel data output\n\
 -e [1e-6|float](--minenergy)  minimum energy level to trigger Russian roulette\n\
 -V [0|1]      (--specular)    1 source located in the background,0 inside mesh\n\
 -k [1|0]      (--voidtime)    when src is outside, 1 enables timer inside void\n\
\n"S_BOLD S_CYAN"\
== GPU options ==\n"S_RESET"\
 -A [0|int]    (--autopilot)   auto thread config:1 enable;0 disable\n\
 -c [opencl,sse,cuda](--compute) select compute backend (default to opencl)\n\
                               can also use 0: sse, 1: opencl, 2: cuda\n\
 -G [0|int]    (--gpu)         specify which GPU to use, list GPU by -L; 0 auto\n\
      or                       if set to -1, CPU-based SSE mmc will be used\n\
 -G '1101'     (--gpu)         using multiple devices (1 enable, 0 disable)\n\
 -W '50,30,20' (--workload)    workload for active devices; normalized by sum\n\
 --atomic [1|0]                1 use atomic operations, 0 use non-atomic ones\n\
\n"S_BOLD S_CYAN"\
== Output options ==\n"S_RESET"\
 -s sessionid  (--session)     a string used to tag all output file names\n\
 -O [X|XFEJLP] (--outputtype)  X - output flux, F - fluence, E - energy density\n\
                               J - Jacobian, L - weighted path length, P -\n\
                               weighted scattering count (J,L,P: replay mode)\n\
 -d [0|1]      (--savedet)     1 to save photon info at detectors,0 not to save\n\
 -H [1000000] (--maxdetphoton) max number of detected photons\n\
 -S [1|0]      (--save2pt)     1 to save the fluence field, 0 do not save\n\
 -x [0|1]      (--saveexit)    1 to save photon exit positions and directions\n\
                               setting -x to 1 also implies setting '-d' to 1\n\
 -X [0|1]      (--saveref)     save diffuse reflectance/transmittance on the \n\
                               exterior surface. The output is stored in a \n\
                               file named *_dref.dat, and the 2nd column of \n\
                               the data is resized to [#Nf, #time_gate] where\n\
                               #Nf is the number of triangles on the surface; \n\
                               #time_gate is the number of total time gates. \n\
                               To plot the surface diffuse reflectance, the \n\
                               output triangle surface mesh can be extracted\n\
                               by faces=faceneighbors(cfg.elem,'rowmajor');\n\
                               where 'faceneighbors' is part of Iso2Mesh.\n\
 -q [0|1]      (--saveseed)    1 save RNG seeds of detected photons for replay\n\
 -F [bin|...] (--outputformat) 'ascii', 'bin' (in 'double'), 'mc2' (double) \n\
                               'hdr' (Analyze) or 'nii' (nifti, double)\n\
                               mc2 - MCX mc2 format (binary 64bit float)\n\
                               jnii - JNIfTI format (https://neurojson.org)\n\
                               bnii - Binary JNIfTI (https://neurojson.org)\n\
                               nii - NIfTI format\n\
                               hdr - Analyze 7.5 hdr/img format\n\
    the bnii/jnii formats support compression (-Z) and generate small files\n\
    load jnii (JSON) and bnii (UBJSON) files using below lightweight libs:\n\
      MATLAB/Octave: JNIfTI toolbox   https://github.com/NeuroJSON/jnifti, \n\
      MATLAB/Octave: JSONLab toolbox  https://github.com/fangq/jsonlab, \n\
      Python:        PyJData:         https://pypi.org/project/jdata\n\
      JavaScript:    JSData:          https://github.com/NeuroJSON/jsdata\n\
 -Z [zlib|...] (--zip)      set compression method if -F jnii or --dumpjson\n\
                            is used (when saving data to JSON/JNIfTI format)\n\
                            0 zlib: zip format (moderate compression,fast) \n\
                            1 gzip: gzip format (compatible with *.gz)\n\
                            2 base64: base64 encoding with no compression\n\
                            3 lzip: lzip format (high compression,very slow)\n\
                            4 lzma: lzma format (high compression,very slow)\n\
                            5 lz4: LZ4 format (low compression,extrem. fast)\n\
                            6 lz4hc: LZ4HC format (moderate compression,fast)\n\
 --dumpjson [-,2,'file.json'] export all settings, including volume data using\n\
                          JSON/JData (https://neurojson.org) format for \n\
                          easy sharing; can be reused using -f\n\
                          if followed by nothing or '-', mmc will print\n\
                          the JSON to the console; write to a file if file\n\
                          name is specified; by default, prints settings\n\
                          after pre-processing; '--dumpjson 2' prints \n\
                          raw inputs before pre-processing\n\
\n"S_BOLD S_CYAN"\
== User IO options ==\n"S_RESET"\
 -h            (--help)        print this message\n\
 -v            (--version)     print MMC version information\n\
 -l            (--log)         print messages to a log file instead\n\
 -i            (--interactive) interactive mode\n\
\n"S_BOLD S_CYAN"\
== Debug options ==\n"S_RESET"\
 -D [0|int]    (--debug)       print debug information (you can use an integer\n\
  or                           or a string by combining the following flags)\n\
 -D [''|SCBWDIOXATRPEM]        1 S  photon movement info\n\
                               2 C  print ray-polygon testing details\n\
                               4 B  print Bary-centric coordinates\n\
                               8 W  print photon weight changes\n\
                              16 D  print distances\n\
                              32 I  entering a triangle\n\
                              64 O  exiting a triangle\n\
                             128 X  hitting an edge\n\
                             256 A  accumulating weights to the mesh\n\
                             512 T  timing information\n\
                            1024 R  debugging reflection\n\
                            2048 P  show progress bar\n\
                            4096 E  exit photon info\n\
                            8192 M  return photon trajectories\n\
      combine multiple items by using a string, or add selected numbers together\n\
 --debugphoton [-1|int]        to print the debug info specified by -D only for\n\
                               a single photon, followed by its index (start 0)\n\
\n"S_BOLD S_CYAN"\
== Additional options ==\n"S_RESET"\
 --momentum     [0|1]          1 to save photon momentum transfer,0 not to save\n\
 --gridsize     [1|float]      if -M G is used, this sets the grid size in mm\n\
 --maxjumpdebug [10000000|int] when trajectory is requested (i.e. -D S),\n\
                               use this parameter to set the maximum positions\n\
                               stored (default: 1e7)\n\
\n"S_BOLD S_CYAN"\
== Example ==\n"S_RESET"\
example: (list built-in benchmarks: -Q/--bench)\n"S_CYAN"\
       %s -Q\n" S_RESET"\
or (use multiple devices - 1st,2nd and 4th GPUs - together with equal load)\n"S_CYAN"\
       %s -Q dmmc-cube60 -n 1e7 -G 1101 -W 10,10,10\n" S_RESET"\
or (use inline json setting modifier)\n"S_CYAN"\
       %s -f input.json -j '{\"Optode\":{\"Source\":{\"Type\":\"isotropic\"}}}'\n" S_RESET"\
or (dump simulation in a single json file)\n"S_CYAN"\
       %s -Q dmmc-cube60 --dumpjson\n" S_RESET"\
or (use -N/--net to browse community-contributed mmc simulations at https://neurojson.io)\n"S_CYAN"\
       %s -N\n" S_RESET"\
or (run user-shared mmc simulations, see full list at https://neurojson.org/db/mmc)\n"S_CYAN"\
       %s -N dmmc-cube60\n" S_RESET"\
or (use -f - to read piped input file modified by shell text processing utilities)\n"S_CYAN"\
       %s -Q dmmc-cube60 --dumpjson | sed -e 's/pencil/isotropic/g' | %s -f -\n" S_RESET"\
or (download/modify simulations from NeuroJSON.io and run with mmc -f)\n"S_CYAN"\
       curl -s -X GET https://neurojson.io:7777/mmc/dmmc-cube60 | jq '.Forward.Dt = 1e-9' | %s -f\n", exename,
#ifdef USE_OPENCL
           'G',
#else
#ifdef MMC_USE_SSE
           'H',
#else
           'P',
#endif
#endif
           exename, exename, exename, exename, exename, exename, exename, exename, exename);
}
