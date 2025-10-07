Mesh-based Monte Carlo (MMC) Trinity - SSE4, OpenCL and CUDA
==============================================


-   Author: Qianqian Fang (q.fang at neu.edu)
-   License: GNU General Public License version 3 (GPL v3), see License.txt
-   Version: 2.8.0 (v2025.10, Bubble Tea)
-   URL: <https://mcx.space/mmc>

![Mex and Binaries](https://github.com/fangq/mcxcl/actions/workflows/build_all.yml/badge.svg)

Table of Content:

  * [What's New](#whats-new)
  * [Introduction](#introduction)
  * [Download and Compile MMC](#download-and-compile-mmc)
  * [Running Simulations](#running-simulations)
    + [Preparation](#preparation)
    + [Command line options](#command-line-options)
    + [Input files](#input-files)
    + [JSON-formatted input files](#json-formatted-input-files)
    + [Photon debugging information using -D flag](#photon-debugging-information-using--d-flag)
    + [Plotting the Results](#plotting-the-results)
  * [Known issues and TODOs](#known-issues-and-todos)
  * [Getting Involved](#getting-involved)
  * [Acknowledgement](#acknowledgement)
    + [SSE Math library by Julien Pommier](#sse-math-library-by-julien-pommier)
    + [cJSON library by Dave Gamble](#cjson-library-by-dave-gamble)
    + [SFMT library by Mutsuo Saito, Makoto Matsumoto and Hiroshima University](#sfmt-library-by-mutsuo-saito--makoto-matsumoto-and-hiroshima-university)
    + [drand48_r port for libgw32c by Free Software Foundation](#drand48-r-port-for-libgw32c-by-free-software-foundation)
    + [git-rcs-keywords by Martin Turon (turon) at Github](#git-rcs-keywords-by-martin-turon--turon--at-github)
  * [Reference](#reference)


What's New
-------------

MMC v2025.10 (2.8.0) is a maintenance release with multiple bug fixes and new features. It is highly
recommended to upgrade for all users.

MMC v2025.10 adds the below key features

* new Python module - pmmc (`pip install pmmc`)
* pmmc supports Apple silicon
* pmmc integrates with pyiso2mesh (`pip install iso2mesh`)
* support focal length via `cfg.srcdir(4)`, fix #108
* support isotropic (`cfg.srcdir(4)=nan`) and Lambertian launch (`cfg.srcdir(4)=-inf`)
* make implicit MMC (immc) available on OpenMP-only builds, such as Apple silicon (where SSE is not supported)
* enable level-3 optimization (`--optlevel 3`) by default, accelerating simulations by 30%-100%

this release also fixed a list of bugs, including

* fix a critical-level bug related to fluence normalization - affect all simulations starting v2025
* fix a "photon-leakage" bug in implicit MMC (immc) #76
* fix the 2nd-neighboring element search in implicit MMC (immc)
* fix incorrect node passing to OpenCL kernel, fix #109
* fix roulettesize type in OpenCL bug, fixed by Aiden Lewis
* remove outdated replay preprocessing, use mcx workflow, fix #110
* allow widefield source to search cfg->e0 first before searching srcelem

The full changelog is listed below

* 2025-10-02 [cc5179c] [doc] update doc for v2025.10, bump pmmc to 0.3.5
* 2025-10-02 [7dd25d9] [bug] enable immc in omp only mode on Apple silicon, fix memory error when r->nexteid
* 2025-10-02 [d21587a] [bug] build immc in omp mode without sse, such as on Apple silicon, result incorrect
* 2025-10-02 [c2cbc3b] [amd] fix one more warning
* 2025-10-02 [7f1b47e] [amd] remove jit warnings
* 2025-10-02 [8d0c19d] [bug] critical bug fix - incorrect energytot and energyesc introduced in commit 072fafe4305cd3c2da78094b0622dc4c67267983 Oct 14 2024
* 2025-10-01 [2f81f87] [clang] silence clang warnings, make optlevel 3 default with macros
* 2025-09-29 [63b48a6] [bug] add the missing gcfg constants when using -o 4 optimization
* 2025-09-27 [2649e36] [doc] update documentation for v2025.10
* 2025-09-19 [74191ba] [ci] avoid using slow choco octave installation
* 2025-09-15 [688c1ba] [feat] support focal length via cfg.srcdir(4), fix #108
* 2025-08-25 [38f8e8a] [pmmc] bump version to 0.3.0 to include expanded mcx utils functions
* 2025-08-21 [c085223] [pmmc] bump pmmc to v0.2.6 to include the bug fixes for immc
* 2025-08-21 [3ab80db] [ci] adjust pybind11 version for various python
* 2025-08-21 [aaf588c] [ci] avoid pybind11 upgrade for python 3.6
* 2025-08-21 [6d93cd6] [ci] fetch tags before checking out specific version
* 2025-08-21 [1d9b118] [ci] downgrade pybind11 to v2 to avoid ci errors
* 2025-08-20 [0c43144] [bug] fix immc photon leakage from edge cases in tracer #76
* 2025-08-18 [0d3cf79] [bug] fix 2nd neighbor element ID referencing, fixing notch artifact thanks to the debugging info provided by Aiden Lewis
* 2025-07-30 [0798ba2] [format] reformat c and matlab with formatters
* 2025-07-30 [f80d541] [bug] remove outdated replay preprocessing, use mcx workflow, fix #110
* 2025-07-19 [f86c824] [ci] upload updated pmmc for apple silicon, bump 0.2.5
* 2025-07-19 [3d6803a] [ci] build pmmc on apple silicon
* 2025-07-19 [97543d3] [pmmc] statically link with libomp, build on apple silicon
* 2025-07-19 [7328d46] [pmmc] use libgomp-1.dll from sparse_numba, bump to 0.2.4
* 2025-07-18 [a808bd1] [ci] update pmmc version to use statically linked _pmmc module
* 2025-07-18 [7c97cf0] [ci] install intel opencl runtime on windows 2022
* 2025-07-18 [8923673] [ci] print verbose info to debug windows pmmc wheel build
* 2025-07-18 [e68a427] [bug] allow widefield source to search cfg->e0 first before searching srcelem
* 2025-07-17 [16031dd] [ci] build windows wheels for python 3.13, bump version 0.2.2
* 2025-07-17 [504730f] [ci] only update pybind11 for 3.13
* 2025-07-17 [542ae94] [ci] check out submodule
* 2025-07-16 [792f809] [ci] use git version of pybind11 for windows pmmc build
* 2025-06-25 [24dab0b] [ci] remove windows python 3.13 build due to errors
* 2025-06-25 [7607095] [pmmc] bump to 0.2.1, add iso2mesh example, add 3.13 on win
* 2025-06-25 [65642f1] [bug] fix roulettesize type in OpenCL bug, fixed by Aiden Lewis
* 2025-06-07 [a69ed21] [pmmc] first fully working version, bump to v0.2.0
* 2025-06-07 [e6b6540] [pmmc] fix pyvista tetgen mesh bug, need to flip element
* 2025-06-07 [40ab888] [pmmc] bump version to 0.1.0
* 2025-06-07 [b104d02] [ci] fix macos pmmc
* 2025-06-07 [031cc06] [ci] macos pmmc build missing omp.h
* 2025-06-07 [8e06991] [ci] fix windows pmmc build
* 2025-06-07 [d747603] [ci] macos pmmc build error
* 2025-06-07 [a3342d8] [ci] remove ubuntu-20.04 from linux, debug macos pmmc build
* 2025-06-07 [98a11dc] [ci] debug windows and macos pmmc build
* 2025-06-07 [f910e0a] [ci] try fixing macos build
* 2025-06-07 [12346b7] [ci] build pmmc
* 2025-06-07 [5ef7a78] [pmmc] version 0.0.1
* 2025-06-06 [edacffb] [pmmc] fix memory error, runs locally
* 2025-06-06 [a537219] [bug] fix some run time bugs, still crashes
* 2025-06-05 [ef159b8] [pmmc] make pmmc module loadable in python3
* 2025-06-05 [09d2499] [pmmc] fix all compilation errors
* 2025-06-05 [eb0a506] [feat] initial units for pmmc
* 2025-05-20 [b85e750] [bug] fix incorrect node passing to OpenCL kernel, fix #109


Introduction
---------------

Mesh-based Monte Carlo (MMC) is a 3D Monte Carlo (MC) simulation software for 
photon transport in complex turbid media. MMC combines the strengths of the 
MC-based technique and the finite-element (FE) method: on the one hand, it can 
handle general media, including low-scattering ones, as in the MC method; on 
the other hand, it can use an FE-like tetrahedral mesh to represent curved 
boundaries and complex structures, making it even more accurate, flexible, and 
memory efficient. MMC uses the state-of-the-art ray-tracing techniques to 
simulate photon propagation in a mesh space. It has been extensively optimized 
for excellent computational efficiency and portability. MMC currently supports 
multi-threaded parallel computing via OpenMP, Single Instruction Multiple Data 
(SIMD) parallelism via SSE and, starting from v2019.10, OpenCL to support a wide 
range of CPUs/GPUs from nearly all vendors.

To run an MMC simulation, one has to prepare an FE mesh first to discretize the 
problem domain. Image-based 3D mesh generation has been a very challenging task 
only until recently. One can now use a powerful yet easy-to-use mesh generator, 
iso2mesh [1], to make tetrahedral meshes directly from volumetric medical 
images. You should download and install the latest iso2mesh toolbox in order to 
run the build-in examples in MMC.

We are working on a massively-parallel version of MMC by porting this code to 
CUDA and OpenCL. This is expected to produce a hundred- or even thousand-fold 
acceleration in speed similar to what we have observed in our GPU-accelerated 
Monte Carlo software (Monte Carlo eXtreme, or MCX [2]).

The most relevant publication describing this work is the GPU-accelerated MMC 
paper:

> Qianqian Fang and Shijie Yan, “GPU-accelerated mesh-based Monte Carlo photon 
transport simulations,” J. of Biomedical Optics, 24(11), 115002 (2019)
URL: <http://dx.doi.org/10.1117/1.JBO.24.11.115002>

Please keep in mind that MMC is only a partial implementation of the general 
Mesh-based Monte Carlo Method (MMCM). The limitations and issues you observed 
in the current software will likely be removed in the future version of the 
software. If you plan to perform comparison studies with other works, please 
communicate with the software author to make sure you have correctly understood 
the details of the implementation.

The details of MMCM can be found in the following paper:

> Qianqian Fang, “Mesh-based Monte Carlo method using fast ray-tracing in 
Plücker coordinates,” Biomed. Opt. Express 1, 165-175 (2010) URL: 
<https://www.osapublishing.org/boe/abstract.cfm?uri=boe-1-1-165>

While the original MMC paper was based on the Plücker coordinates, a number of 
more efficient SIMD-based ray-tracers, namely, Havel SSE4 ray-tracer, Badouel 
SSE ray-tracer and branchless-Badouel SSE ray-tracer (fastest) have been added 
since 2011. These methods can be selected by the `-M` flag. The details of these 
methods can be found in the below paper

> Qianqian Fang and David R. Kaeli, “Accelerating mesh-based Monte Carlo method 
on modern CPU architectures,” Biomed. Opt. Express 3(12), 3223-3230 (2012) 
URL: <https://www.osapublishing.org/boe/abstract.cfm?uri=boe-3-12-3223>

and their key differences compared to another mesh-based MC simulator, TIM-OS, 
are discussed in

> Qianqian Fang, “Comment on 'A study on tetrahedron-based inhomogeneous 
Monte-Carlo optical simulation',” Biomed. Opt. Express, vol. 2(5) 1258-1264, (2011)
URL: <https://www.osapublishing.org/boe/abstract.cfm?uri=boe-2-5-1258>

In addition, the generalized MMC algorithm for wide-field sources and detectors 
are described in the following paper, and was made possible with the 
collaboration with Ruoyang Yao and Prof. Xavier Intes from RPI

> Yao R, Intes X, Fang Q, “Generalized mesh-based Monte Carlo for wide-field 
illumination and detection via mesh retessellation,” Biomed. Optics Express, 
7(1), 171-184 (2016) URL: 
<https://www.osapublishing.org/boe/abstract.cfm?uri=boe-7-1-171>

In addition, we have been developing a fast approach to build the Jacobian 
matrix for solving inverse problems. The technique is called “photon 
replay”, and is described in details in the below paper:

> Yao R, Intes X, Fang Q, “A direct approach to compute Jacobians for diffuse 
optical tomography using perturbation Monte Carlo-based photon 'replay',” 
Biomed. Optics Express, in press, (2018)

In 2019, we published an improved MMC algorithm, named “dual-grid MMC”, or 
DMMC, in the below JBO Letter. This method allows to use separate mesh for 
ray-tracing and fluence storage, and can be 2 to 3 fold faster than the 
original MMC without loss of accuracy.

> Shijie Yan, Anh Phong Tran, Qianqian Fang, “A dual-grid mesh-based Monte 
Carlo algorithm for efficient photon1transport simulations in complex 3-D 
media,” J. of Biomedical Optics, 24(2), 020503 (2019).

The authors of the papers are greatly appreciated if you can cite the above 
papers as references if you use MMC and related software in your publication.


Download and Compile MMC
---------------------------

The latest release of MMC can be downloaded from the following URL:

<https://mcx.space/#mmc>

The development branch (not fully tested) of the code can be accessed using 
Git. However this is not encouraged unless you are a developer. To check out 
the Git source code, you should use the following command:

      git clone https://github.com/fangq/mmc.git

To compile the software, you need to install GNU gcc compiler toolchain on your 
system. For Debian/Ubuntu based GNU/Linux systems, you can type

      sudo apt-get install build-essential

and for Fedora/Redhat based GNU/Linux systems, you can type

      sudo dnf install make automake gcc gcc-c++

To compile the binary with multi-threaded computing via OpenMP, 
your `gcc` version should be at least 4.0. To compile the binary 
supporting SSE4 instructions, gcc version should be at least 
4.3.4. For windows users, you should install MSYS2 or Cygwin64 [3]. During the 
installation, please select `mingw64-x86_64-gcc` and `make` packages.
For MacOS users, you need to install the newer gcc from 
Homebrew or MacPorts and use the instructions below to compile the 
MMC source code.

## Building MMC using CMake

One can choose one of the two methods to build mmc binaries. The first
approach is to use CMake. CMake is a portable system creating compilation
and linking commands automatically adapted to your operating system and 
installed compiler. It can run on Linux, MacOS and Windows.

To use CMake, you will have to first run `sudo apt-get install cmake`
or `sudo dnf install cmake` to install cmake first. To build MMC binaries,
you should first navigate to the `mmc/src` folder, and run

      mkdir -p build
      cd build
      cmake .. && make

if cmake complains that any required library is missing, you will need
to install those dependencies, removing all files inside the build folder,
and run the cmake command above again.

The above command builds the `mmc` executable inside `mmc/bin` folder. If
your system has MATLAB installed, the above command also builds mmclab mex
file as `mmc/mmclab/mmc.mex*` where the mex suffix depends on your OS.

If you want to build the "Trinity" version of mmc to support CUDA on NVIDIA
GPUs, you will have to first install CUDA toolkit, and replace the above cmake
command by 

      cmake .. -DBUILD_CUDA=on && make

the executable will be build as `mmc/bin/mmciii` and the mex file is `mmc/mmclab/mmciii.mex*`.


## Building MMC using GNU Make

A more traditional, and fine-grained, approach to build MMC is to use the provided
`Makefile` using GNU make. Similarly, you will need to open a terminal, navigate to
the `mmc/src` folder, and type

      make

this should create a fully optimized OpenCL based mmc executable, located under 
the `mmc/bin/` folder. The binary also supports SSE4 on the CPU.

Other compilation options include

      make ssemath  # this is the same as make, building mmc binary with SSE4+OpenMP+OpenCL
      make cuda     # this compiles the "Trinity" version of mmc, supports SSE4+OpenMP+OpenCL+CUDA
      make omp      # this compiles a multi-threaded binary using OpenMP
      make release  # create a single-threaded optimized binary
      make prof     # this makes a binary to produce profiling info for gprof
      make sse      # this uses SSE4 for all vector operations (dot, cross), implies omp

if you wish to build the mmc mex file to be used in matlab, you should run

      make mex      # this produces mmc.mex* under mmc/mmclab/ folder
      make cudamex  # this produces a "Trinity" version of mmc.mex* that supports SSE+OpenCL+CUDA

similarly, if you wish to build the mex file for GNU Octave, you should run

      make oct      # this produces mmc.mex* under mmc/mmclab/ folder
      make cudaoct  # this produces a "Trinity" version of mmc.mex* that supports SSE+OpenCL+CUDA

If you append `-f makefile_sfmt` at the end of any of the above make 
commands, you will get an executable named `mmc_sfmt`, which uses a fast 
MT19937 random-number-generator (RNG) instead of the default GLIBC 48bit RNG. 
If your CPU supports SSE4, the fastest binary can be obtained by running the 
following command:

      make ssemath -f makefile_sfmt

You should be able to compile the code with an Intel C++ compiler, an AMD C 
compiler or LLVM compiler without any difficulty. To use other compilers, you 
simply append `CC=compiler_exe` to the above make commands. If you see any 
error messages, please google and fix your compiler settings or install the 
missing libraries.

A special note for Mac OS users: you can you use both gcc (installed by MacPorts
or brew) or the default clang gcc provided by the default Xcode compiler to build
mmc. MMC requires OpenMP for multi-threading based parallel computing. If one uses
the clang compiler, one must first install `libomp` package in order to compile mmc.

      brew install libomp
      brew link --force libomp

One can switch to other compilers by setting the `CC`, `CXX` and `AR` environment
variables, for example

      make CC=gcc-mp-10 CXX=g++-mp-10 AR=g++-mp-10

After compilation, you may add the path to the `mmc` binary (typically, 
`mmc/bin`) to your search path. To do so, you should modify your `$PATH` 
environment variable. Detailed instructions can be found at [5].

You can also compile MMC using Intel's C++ compiler - `icc`. To do this, you run

      make CC=icc

you must enable `icc` related environment variables by source the `compilervars.sh`
file. The speed of icc-generated mmc binary is generally faster for CPU/SSE based
MMC simulation than those compiled by `gcc`.


Running Simulations
----------------------

### Preparation

Before you create/run your own MMC simulations, we suggest you first 
understanding all the examples under the mmc/example directory, checking out 
the formats of the input files and the scripts for pre- and post-processing.

Because MMC uses FE meshes in the simulation, you should create a mesh for your 
problem domain before launching any simulation. This can be done fairly 
straightforwardly using a Matlab/Octave mesh generator, iso2mesh [1], 
developed by the MMC author. In the mmc/matlab folder, we also provide 
additional functions to generate regular grid-shaped tetrahedral meshes.

It is required to use the `savemmcmesh` function under the mmc/matlab 
folder to save the mesh output from iso2mesh, because it performs additional 
tests to ensure the consistency of element orientations. If you choose not to 
use `savemmcmesh`, you MUST call the `meshreorient` function in iso2mesh to 
test the `elem` array and make sure all elements are oriented in the same 
direction. Otherwise, MMC will give incorrect results.

### Command line options

The full command line options of MMC include the following:

```
###############################################################################
#                     Mesh-based Monte Carlo (MMC) - OpenCL                   #
#          Copyright (c) 2010-2025 Qianqian Fang <q.fang at neu.edu>          #
#              https://mcx.space/#mmc  &  https://neurojson.io/               #
#                                                                             #
#Computational Optics & Translational Imaging (COTI) Lab  [http://fanglab.org]#
#   Department of Bioengineering, Northeastern University, Boston, MA, USA    #
###############################################################################
#    The MCX Project is funded by the NIH/NIGMS under grant R01-GM114365      #
###############################################################################
#  Open-source codes and reusable scientific data are essential for research, #
# MCX proudly developed human-readable JSON-based data formats for easy reuse.#
#                                                                             #
#Please visit our free scientific data sharing portal at https://neurojson.io/#
# and consider sharing your public datasets in standardized JSON/JData format #
###############################################################################
$Rev::      $v2025.10$Date::                       $ by $Author::             $
###############################################################################

usage: mmc <param1> <param2> ...
where possible parameters include (the first item in [] is the default value)

== Required option ==
 -f config     (--input)       read an input file in .json or inp format

== MC options ==
 -n [0.|float] (--photon)      total photon number, max allowed value is 2^32-1
 -b [0|1]      (--reflect)     1 do reflection at int&ext boundaries, 0 no ref.
 -U [1|0]      (--normalize)   1 to normalize the fluence to unitary,0 save raw
 -m [0|1]      (--mc)          0 use MCX-styled MC method, 1 use MCML style MC
 -C [1|0]      (--basisorder)  1 piece-wise-linear basis for fluence,0 constant
 -u [1.|float] (--unitinmm)    define the mesh data length unit in mm
 -E [1648335518|int|mch](--seed) set random-number-generator seed;
                               if an mch file is followed, MMC "replays" 
                               the detected photons; the replay mode can be used
                               to calculate the mua/mus Jacobian matrices
 -P [0|int]    (--replaydet)   replay only the detected photons from a given 
                               detector (det ID starts from 1), use with -E 
 -M [G|SG]    (--method)      choose ray-tracing algorithm (only use 1 letter)
                               P - Plucker-coordinate ray-tracing algorithm
                               H - Havel's SSE4 ray-tracing algorithm
                               B - partial Badouel's method (used by TIM-OS)
                               S - branch-less Badouel's method with SSE
                               G - dual-grid MMC (DMMC) with voxel data output
 -e [1e-6|float](--minenergy)  minimum energy level to trigger Russian roulette
 -V [0|1]      (--specular)    1 source located in the background,0 inside mesh
 -k [1|0]      (--voidtime)    when src is outside, 1 enables timer inside void

== GPU options ==
 -A [0|int]    (--autopilot)   auto thread config:1 enable;0 disable
 -c [opencl,sse,cuda](--compute) select compute backend (default to opencl)
                               can also use 0: sse, 1: opencl, 2: cuda
 -G [0|int]    (--gpu)         specify which GPU to use, list GPU by -L; 0 auto
      or                       if set to -1, CPU-based SSE mmc will be used
 -G '1101'     (--gpu)         using multiple devices (1 enable, 0 disable)
 -W '50,30,20' (--workload)    workload for active devices; normalized by sum
 --atomic [1|0]                1 use atomic operations, 0 use non-atomic ones

== Output options ==
 -s sessionid  (--session)     a string used to tag all output file names
 -O [X|XFEJLP] (--outputtype)  X - output flux, F - fluence, E - energy density
                               J - Jacobian, L - weighted path length, P -
                               weighted scattering count (J,L,P: replay mode)
 -d [0|1]      (--savedet)     1 to save photon info at detectors,0 not to save
 -H [1000000] (--maxdetphoton) max number of detected photons
 -S [1|0]      (--save2pt)     1 to save the fluence field, 0 do not save
 -x [0|1]      (--saveexit)    1 to save photon exit positions and directions
                               setting -x to 1 also implies setting '-d' to 1
 -X [0|1]      (--saveref)     save diffuse reflectance/transmittance on the 
                               exterior surface. The output is stored in a 
                               file named *_dref.dat, and the 2nd column of 
                               the data is resized to [#Nf, #time_gate] where
                               #Nf is the number of triangles on the surface; 
                               #time_gate is the number of total time gates. 
                               To plot the surface diffuse reflectance, the 
                               output triangle surface mesh can be extracted
                               by faces=faceneighbors(cfg.elem,'rowmajor');
                               where 'faceneighbors' is part of Iso2Mesh.
 -q [0|1]      (--saveseed)    1 save RNG seeds of detected photons for replay
 -F [bin|...] (--outputformat) 'ascii', 'bin' (in 'double'), 'mc2' (double) 
                               'hdr' (Analyze) or 'nii' (nifti, double)
                               mc2 - MCX mc2 format (binary 64bit float)
                               jnii - JNIfTI format (https://neurojson.org)
                               bnii - Binary JNIfTI (https://neurojson.org)
                               nii - NIfTI format
                               hdr - Analyze 7.5 hdr/img format
    the bnii/jnii formats support compression (-Z) and generate small files
    load jnii (JSON) and bnii (UBJSON) files using below lightweight libs:
      MATLAB/Octave: JNIfTI toolbox   https://github.com/NeuroJSON/jnifti, 
      MATLAB/Octave: JSONLab toolbox  https://github.com/fangq/jsonlab, 
      Python:        PyJData:         https://pypi.org/project/jdata
      JavaScript:    JSData:          https://github.com/NeuroJSON/jsdata
 -Z [zlib|...] (--zip)      set compression method if -F jnii or --dumpjson
                            is used (when saving data to JSON/JNIfTI format)
                            0 zlib: zip format (moderate compression,fast) 
                            1 gzip: gzip format (compatible with *.gz)
                            2 base64: base64 encoding with no compression
                            3 lzip: lzip format (high compression,very slow)
                            4 lzma: lzma format (high compression,very slow)
                            5 lz4: LZ4 format (low compression,extrem. fast)
                            6 lz4hc: LZ4HC format (moderate compression,fast)
 --dumpjson [-,2,'file.json'] export all settings, including volume data using
                          JSON/JData (https://neurojson.org) format for 
                          easy sharing; can be reused using -f
                          if followed by nothing or '-', mmc will print
                          the JSON to the console; write to a file if file
                          name is specified; by default, prints settings
                          after pre-processing; '--dumpjson 2' prints 
                          raw inputs before pre-processing

== User IO options ==
 -h            (--help)        print this message
 -v            (--version)     print MMC version information
 -l            (--log)         print messages to a log file instead
 -i            (--interactive) interactive mode

== Debug options ==
 -D [0|int]    (--debug)       print debug information (you can use an integer
  or                           or a string by combining the following flags)
 -D [''|SCBWDIOXATRPEM]        1 S  photon movement info
                               2 C  print ray-polygon testing details
                               4 B  print Bary-centric coordinates
                               8 W  print photon weight changes
                              16 D  print distances
                              32 I  entering a triangle
                              64 O  exiting a triangle
                             128 X  hitting an edge
                             256 A  accumulating weights to the mesh
                             512 T  timing information
                            1024 R  debugging reflection
                            2048 P  show progress bar
                            4096 E  exit photon info
                            8192 M  return photon trajectories
      combine multiple items by using a string, or add selected numbers together
 --debugphoton [-1|int]        to print the debug info specified by -D only for
                               a single photon, followed by its index (start 0)

== Additional options ==
 --momentum     [0|1]          1 to save photon momentum transfer,0 not to save
 --gridsize     [1|float]      if -M G is used, this sets the grid size in mm
 --maxjumpdebug [10000000|int] when trajectory is requested (i.e. -D S),
                               use this parameter to set the maximum positions
                               stored (default: 1e7)

== Example ==
       mmc -n 1000000 -f input.json -s test -b 0 -D TP -G -1
```

### Input files

It is highly recommended to use the JSON-formatted input file described in the following
section. The legacy input file format `.inp` is depreciated and may be removed in
future releases.

The simplest example can be found under the `example/onecube` folder. 
Please run `createmesh.m` first from Matlab/Octave to create all the mesh 
files, which include

    elem_onecube.dat    -- tetrahedral element file
    facenb_onecube.dat  -- element neighbors of each face
    node_onecube.dat    -- node coordinates
    prop_onecube.dat    -- optical properties of each element type
    velem_onecube.dat   -- volume of each element

The input file of the example is named `onecube.inp`, where we specify most 
of the simulation parameters. The input file follows a similar format as in 
MCX, which looks like the following

    100                  # total photon number (can be overwriten by -n)
    17182818             # RNG seed, negative to regenerate
    2. 8. 0.             # source position (mm)
    0. 0. 1.             # initial incident vector
    0.e+00 5.e-09 5e-10  # time-gates(s): start, end, step
    onecube              # mesh id: name stub to all mesh files
    3                    # index of element (starting from 1) which encloses the source
    3       1.0          # detector number and radius (mm)
    2.0     6.0    0.0   # detector 1 position (mm)
    2.0     4.0    0.0   # ...
    2.0     2.0    0.0
    pencil               # optional: source type
    0 0 0 0              # optional: source parameter set 1
    0 0 0 0              # optional: source parameter set 2

The mesh files are linked through the `mesh id` (a name stub) with a format 
of `{node|elem|facenb|velem}_meshid.dat`. All mesh files must exist for an MMC 
simulation. If the index to the tetrahedron that encloses the source is not 
known, please use the `tsearchn` function in matlab/octave to find out and 
supply it in the 7th line in the input file. Examples are provided in 
`mmc/examples/meshtest/createmesh.m`.

To run a simulation, you should execute the `run_test.sh` bash script in 
this folder. If you want to run mmc directly from the command line, you can do 
so by typing

`../../bin/mmc -n 20 -f onecube.inp -s onecube`

where `-n` specifies the total photon number to be simulated, `-f` specifies the 
input file, and `-s` gives the output file name. To see all the supported 
options, run `mmc` without any parameters.

The above command only simulates 20 photons and will complete instantly. An 
output file `onecube.dat` will be saved to record the normalized (unitary) 
flux at each node. If one specifies multiple time-windows from the input file, 
the output will contain multiple blocks with each block corresponding to the 
time-domain solution at all nodes computed for each time window.

More sophisticated examples can be found under the `example/validation` and 
`example/meshtest` folders, where you can find `createmesh` scripts and 
post-processing script to make plots from the simulation results.

### JSON-formatted input files

Starting from version 0.9, MMC accepts a JSON-formatted input file in addition 
to the conventional tMCimg-like input format. JSON (JavaScript Object Notation) 
is a portable, human-readable and “fat-free” text format to represent 
complex and hierarchical data. Using the JSON format makes a input file 
self-explanatory, extensible and easy-to-interface with other applications 
(like MATLAB).

A sample JSON input file can be found under the `examples/onecube` folder. The 
same file, `onecube.json`, is also shown below:

    {
        "Domain": {
            "MeshID": "onecube",
            "InitElem": 3
        },
        "Session": {
            "Photons":  100,
            "Seed":     17182818,
            "ID":       "onecube"
        },
        "Forward": {
            "T0": 0.0e+00,
            "T1": 5.0e-09,
            "Dt": 5.0e-10
        },
        "Optode": {
            "Source": {
                "Type": "pencil",
                "Pos": [2.0, 8.0, 0.0],
                "Dir": [0.0, 0.0, 1.0],
                "Param1": [0.0, 0.0, 0.0, 0.0],
                "Param2": [0.0, 0.0, 0.0, 0.0]
            },
            "Detector": [
                {
                    "Pos": [2.0, 6.0, 0.0],
                    "R": 1.0
                },
                {
                    "Pos": [2.0, 4.0, 0.0],
                    "R": 1.0
                },
                {
                    "Pos": [2.0, 2.0, 0.0],
                    "R": 1.0
                }
            ]
        }
    }

A JSON input file requires 4 root objects, namely `Domain`, `Session`, 
`Forward` and `Optode`. Each object is a data structure providing 
information as indicated by its name. Each object can contain various 
sub-fields. The orders of the fields in the same level are flexible. For each 
field, you can always find the equivalent fields in the `*.inp` input files. For 
example, The `MeshID` field under the `Mesh` object is the same as 
Line\#6 in onecube.inp; the `InitElem` under `Mesh` is the same as 
Line\#7; the `Forward.T0` is the same as the first number in Line\#5, etc.

An MMC JSON input file must be a valid JSON text file. You can validate your 
input file by running a JSON validator, for example <http://jsonlint.com/> You 
should always use `...` to quote a `name` and separate parallel items 
by `,`.

MMC accepts an alternative form of JSON input, but using it is not recommended. 
In the alternative format, you can use `"rootobj_name.field_name": value `

to represent any parameter directly in the root level. For example

    {
        "Domain.MeshID": "onecube",
        "Session.ID": "onecube",
        ...
    }

You can even mix the alternative format with the standard format. If any input 
parameter has values in both formats in a single input file, the 
standard-formatted value has higher priority.

To invoke the JSON-formatted input file in your simulations, you can use the 
`-f` command line option with MMC, just like using an `.inp` file. For 
example:

      ../../bin/mmc -n 20 -f onecube.json -s onecubejson -D M

The input file must have a `.json` suffix in order for MMC to recognize. If 
the input information is set in both command line, and input file, the command 
line value has higher priority (this is the same for `.inp` input files). For 
example, when using `-n 20`, the value set in `Session`/`Photons` 
is overwritten to 20; when using `-s onecubejson`, the 
`Session`/`ID` value is modified. If your JSON input file is invalid, 
MMC will quit and point out where it expects you to double check.

### Photon debugging information using -D flag

the output format for `-D M` (photon moving) is below:

    ? px py pz eid id scat

    ? is a single letter representing the state of the current position:
       B a boundary point
       P the photon is passing an interface point
       T the photon terminates at this location due to
          exceeding end of the time window
       M a position other than any of the above

    px,py,pz: the current photon position

    eid: the index (starting from 1) of the current enclosing element

    id: the index of the current photon, from 1 to nphoton

    scat: the "normalized" length to read the next scattering site, \
       it is unitless

    for -D A (flux accumulation debugging), the output is

    A ax ay az ww eid dlen

    ax ay az: the location where the accumulation calculation was done \
       (typically, the half-way point of the line segment between the last \
       and current positions)

    ww: the photon weight loss for the line segment

    dlen=scat/mus of the current element: the distance left to arrive \
       the next scattering site

    for -D E

    E  px py pz vx vy vz w eid

    vx vy vz: the unitary propagation vector when the photon exits
    w: the current photon weight


### Plotting the Results

As described above, MMC produces a fluence-rate output file as 
`session-id.dat`. By default, this file contains the normalized, i.e. under 
unitary source, fluence at each node of the mesh. The detailed interpretation 
of the output data can be found in [6]. If multiple time-windows are defined, 
the output file will contain multiple blocks of data, with each block being the 
fluence distribution at each node at the center point of each time-window. The 
total number of blocks equals to the total time-gate number.

To read the mesh files (tetrahedral elements and nodes) into matlab, one can 
use `readmmcnode` and `readmmcelem` function under the mmc/matlab directory. 
Plotting non-structural meshes in matlab is possible with interpolation 
functions such as griddata3. However, it is very time-consuming for large 
meshes. In iso2mesh, a fast mesh slicing & plotting function, `qmeshcut`, is very 
efficient in making 3D plots of mesh or cross-sections. More details can be 
found at this webpage [7], or `help qmeshcut` in matlab. Another useful 
function is plotmesh in iso2mesh toolbox. It has very flexible syntax to allow 
users to plot surfaces, volumetric meshes and cross-section plots. One can use 
something like

      plotmesh([node fluence],elem,'x<30 & y>30');

to plot a sliced mesh, or

      plotmesh([node log10(fluence)],elem,'x=30'); view(3)

to show a cross-sectional plot.

Please edit or browse the `*.m` files under all example subfolder to find more 
options to make plot from MMC output.

When users specify `-d 1` to record partial path lengths for all detected 
photons, an output file named `sessionid.mch` will be saved under the same 
folder. This file can be loaded into Matlab/Octave using the `loadmch.m`
script under the mmc/matlab folder. The output of loadmch script has the 
following columns:

      detector-id, scattering-events, partial-length_1, partial-length_2, ...., additional data ...`

The simulation settings will be returned by a structure. Using the information 
from the mch file will allow you to re-scale the detector readings without 
rerunning the simulation (for absorption changes only).


Known issues and TODOs
-------------------------

-   MMC only supports linear tetrahedral elements at this point. 
 Quadratic elements will be added later
-   Currently, this code only supports element-based optical properties; 
 nodal-based optical properties (for continuously varying media) will be
 added in a future release


Getting Involved
-------------------

MMC is an open-source software. It is released under the terms of GNU General 
Public License version 3 (GPLv3). That means not only everyone can download and 
use MMC for any purposes, but also you can modify the code and share the 
improved software with others (as long as the derived work is also licensed 
under the GPLv3 license).

If you already made a change to the source code to fix a bug you encountered in 
your research, we are appreciated if you can share your changes (as `git diff` 
outputs) with the developers. We will patch the code as soon as we 
fully test the changes (we will acknowledge your contribution in the MMC 
documentation).

When making edits to the source code with an intent of sharing with the 
upstream authors, please set your editor's tab width to 8 so that the 
indentation of the source is correctly displayed. Please keep your patch as 
small and local as possible, so that other parts of the code are not influenced.

To streamline the process process, the best way to contribute your patch is to 
click the **fork** button from <http://github.com/fangq/mmc>, and then change 
the code in your forked repository. Once fully tested and documented, you can 
then create a **pull request** so that the upstream author can review the 
changes and accept your change.

In you are a user, please use our mmc-users mailing list to post questions or 
share experience regarding MMC. The mailing lists can be found from this link:

<https://mcx.space/#about>


Acknowledgement
------------------

MMC uses the following open-source libraries:


### ZMat data compression unit

- Files: src/zmat/*
- Copyright: 2019-2020 Qianqian Fang
- URL: https://github.com/fangq/zmat
- License: GPL version 3 or later, https://github.com/fangq/zmat/blob/master/LICENSE.txt

### LZ4 data compression library

- Files: src/zmat/lz4/*
- Copyright: 2011-2020, Yann Collet
- URL: https://github.com/lz4/lz4
- License: BSD-2-clause, https://github.com/lz4/lz4/blob/dev/lib/LICENSE

### LZMA/Easylzma data compression library

- Files: src/zmat/easylzma/*
- Copyright: 2009, Lloyd Hilaiel, 2008, Igor Pavlov
- License: public-domain
- Comment:
 All the cruft you find here is public domain.  You don't have to
 credit anyone to use this code, but my personal request is that you mention
 Igor Pavlov for his hard, high quality work.

### Miniz compression library

- Files: src/zmat/miniz/*
- Copyright 2013-2014 RAD Game Tools and Valve Software
- Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC
- URL: https://github.com/richgel999/miniz
- License: MIT-license, https://github.com/richgel999/miniz/blob/master/LICENSE

### SSE Math library by Julien Pommier

Copyright (C) 2007 Julien Pommier

This software is provided 'as-is', without any express or implied warranty. In 
no event will the authors be held liable for any damages arising from the use 
of this software.

Permission is granted to anyone to use this software for any purpose, including 
commercial applications, and to alter it and redistribute it freely, subject to 
the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim 
that you wrote the original software. If you use this software in a product, an 
acknowledgment in the product documentation would be appreciated but is not 
required.

2. Altered source versions must be plainly marked as such, and must not be 
misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.

(this is the zlib license)

### cJSON library by Dave Gamble

Copyright (c) 2009 Dave Gamble

Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the “Software”), to deal 
in the Software without restriction, including without limitation the rights to 
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
of the Software, and to permit persons to whom the Software is furnished to do 
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE.

### SFMT library by Mutsuo Saito, Makoto Matsumoto and Hiroshima University

Copyright (c) 2006,2007 Mutsuo Saito, Makoto Matsumoto and Hiroshima 
University. All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

-   Redistributions of source code must retain the above copyright 
notice, this list of conditions and the following disclaimer. 

-   Redistributions in binary form must reproduce the above 
copyright notice, this list of conditions and the following 
disclaimer in the documentation and/or other materials provided 
with the distribution.

-   Neither the name of the Hiroshima University nor the names of
its contributors may be used to endorse or promote products 
derived from this software without specific prior written 
permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR 
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

### drand48_r port for libgw32c by Free Software Foundation

Copyright (C) 1995, 1997, 2001 Free Software Foundation, Inc. This file is part 
of the GNU C Library. Contributed by Ulrich Drepper 
&lt;drepper@gnu.ai.mit.edu&gt;, August 1995.

The GNU C Library is free software; you can redistribute it and/or modify it 
under the terms of the GNU Lesser General Public License as published by the 
Free Software Foundation; either version 2.1 of the License, or (at your 
option) any later version.

The GNU C Library is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for 
more details.

You should have received a copy of the GNU Lesser General Public License along 
with the GNU C Library; if not, write to the Free Software Foundation, Inc., 59 
Temple Place, Suite 330, Boston, MA 02111-1307 USA.

### git-rcs-keywords by Martin Turon (turon) at Github

MMC includes a pair of git filters (`.git_filters/rcs-keywords.clean` and 
`.git_filters/rcs-keywords.smudge`) to automatically update SVN keywords in 
`mcx_utils.c`. The two simple filter scripts were licensed under the BSD license 
according to this link:

<https://github.com/turon/git-rcs-keywords/issues/4>

Both filter files were significantly modified by Qianqian Fang.


Reference
------------


- [1] <http://iso2mesh.sf.net> -- an image-based surface/volumetric mesh generator 
- [2] <https://mcx.space> -- Monte Carlo eXtreme: a GPU-accelerated MC code 
- [3] <https://cygwin.com/setup-x86_64.exe> 
- [4] <http://developer.apple.com/mac/library/releasenotes/DeveloperTools/RN-llvm-gcc/index.html> 
- [5] <http://iso2mesh.sourceforge.net/cgi-bin/index.cgi?Doc/AddPath> 
- [6] <https://mcx.space/cgi-bin/index.cgi?MMC/Doc/FAQ#How_do_I_interpret_MMC_s_output_data> 
- [7] <http://iso2mesh.sourceforge.net/cgi-bin/index.cgi?fun/qmeshcut>
