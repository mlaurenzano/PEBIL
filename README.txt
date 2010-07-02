PEBIL (pebil) - README 
2.2	Binary Instrumentation Packages for Gathering Computational Trace
The PMaC Prediction Framework relies on binary instrumentation to gather information about the computational work done during an application run. The information in a computational trace is the floating point operation count, memory operation count, and simulated cache hit rates (stored per basic block for each level of the caches in the memory hierarchy) of the target machine(s). Each basic block in an application is given a unique ID that is assigned to it in a consistent manner during each instrumentation pass. The unique ID for every basic block in the application is consistent as long as the executable is kept the same . The simulated cache hit rates are computed on-the-fly during the trace by inputing the address stream of the instrumented running application into a cache simulator. The specifications for the cache or caches to be simulated are specified in the CacheStructures.h and describe in more detail in section 2.2.1.3.
To gather computation traces, PMaC provides two instrumentation tools. PEBIL is a binary editing tool for X86/Linux systems (described in this document) and PMaCInstrumentor is a binary editing tool for IBM Power/AIX systems.
2.2.1	PEBIL 
PEBIL  is a binary instrumentation toolkit that operates on ELF binaries on Linux for x86/x86_64 processors. PEBIL has a C++ API that provides the means to inject code and data into a binary file. It also provides several pre-implemented tools for basic block counting and cache simulation for a set of memory hierarchies.
2.2.1.1	Installation for Application Tracing
Host Systems: Any X86/Linux
The source code for the PEBIL library and instrumentation tools is available at through PMaC Laboratories by e-mailing help@pmaclabs.com. 
The directory structure of distribution is as follows:
$ cd /home/userid/tools/PEBIL
$ ls
bin/   include/   instcode/    Makefile    scripts/    testapps/    tools/   docs/    etc/    lib/    src/ 
The include, src, tools, and instcode directories contain the source code for instrumentation related libraries and instrumentation tools. The bin and lib directories contain the executables and shared libraries after building the distribution. The scripts directory contains additional PERL scripts to assist application tracing for high level instrumentation activities.
After retrieving the source, users can change to the top directory of the distribution and follow the instructions in the INSTALL to build the distribution. After building the executables and shared libraries, it is also advised to link or copy the scripts in the scripts directory to the bin directory of the distribution so that the scripts will be found in user's path as well. The following is an example of this procedure:
$ cd /home/userid/tools/PEBIL
$ gmake clean
$ gmake
$ cd bin
$ ln -s ../scripts/selectSimBlocks.pl  selectSimBlocks.pl
$ ln -s ../scripts/traceHelper.pl traceHelper.pl
The Makefile at the top directory /home/userid/tools/PEBIL iterate over sub directories and run the appropriate make commands. Alternatively, users can follow the same steps manually. Instead of gmake at the top directory, the users can run the following. Note that the order of making src should come before the order of making tools directory. For an installed distribution to work, minimal set of steps is to make src, tools and instcode directories.
$ cd /home/userid/tools/PEBIL
$ cd ./src
$ gmake
$ cd ../tools
$ gmake
$ cd ../instcode
$ gmake
Manually following the make steps is particularly useful when the default configuration (configuration that comes with the source code) is not used for building the instcode directory. There are various ways of building the code under this directory and the available compilation macros are described in details in Section 2.2.1.2.
To use PEBIL, users need to include the bin directory of distribution to their path.  Thus, if a particular distribution needs to be used, the user needs to set the path variables accordingly before using that distribution as the name of the executable and additional PERL scripts would be the same. When installed properly, the PEBIL executable, pebil, should be in user's path. It can be checked using the which command of Unix from a different directory (you may need to source .cshrc/.tcshrc or .profile/.bashrc files for paths to be adjusted). The --help option will print a brief usage information.
$ source ~/.cshrc
$ which pebil
/home/userid/tools/PEBIL/bin/pebil
$ pebil --help
   usage : pebil
        --typ (ide|dat|bbt|cnt|jbb|sim|csc)
        --app <executable_path>
        --inp <block_unique_ids>    <-- valid for sim/csc
        [--lib <shared_lib_topdir>]
        [--ext <output_suffix>]
        [--dtl]
        [--lpi]                                    <-- valid for sim/csc
        [--phs <phase_no>]            <-- valid for sim/csc
        [--dfp <pattern_file>]           <-- valid for sim/csc
        [--help]
Brief Descriptions for Options:
===============================
        --typ : required for all. Instrumentation type.
        --app : required for all. Executable path.
        --lib : optional for all. Instrumentation shared library top
                directory. default is $PEBIL_LIB
        --ext : optional for all. File extension for output files.
                default is (typ)inst, such as jbbinst for type jbb.
        --dtl : optional for all. Enable detailed .static file with line numbers
                and filenames. default is no details.
        --inp : required for sim/csc. File including list of block ids to instrument
        --lpi : optional for sim/csc. Loop level block inclusion for
                cache simulation. default is no.
        --phs : optional for sim/csc. Phase number. defaults to no phase,
                otherwise, .phase.N. is included in output file names
        --dfp : optional for sim/csc. dfpattern file. defaults to no dfpattern file,
2.2.1.2	Installation for MultiMAPS Tracing
Host Systems: Any X86/Linux
In the framework, the MultiMAPS data for a target system needs to be augmented with the cooresponding hit rates. To accomplish this, MultiMAPS needs to be traced with the target systems cache structure. The default PEBIL installation, which comes configured for application tracing, uses address stream sampling for cache simulation. Rather than using every address in the address stream of an application run, it uses only a subset of addresses in the address stream and simulates them for the target memory subsystems. This method of sampling the address stream has been used to reduce the overhead of instrumentation.  For MultiMAPS tracing, however, the entire address stream needs to be consumed for cache simulations, therefore sampling should not be used. Hence, separate installations of PEBIL are needed for tracing MultiMAPS for cache hit rates. 
To install PEBIL for MultiMAPS tracing, you need to edit the instcode/Makefile file under the PEBIL installation directory (a different directory than the one installed for application tracing) and repeat the installation process described in Section 2.2.1.1 for this new directory. To install pebil for MultiMAPS tracing, user needs to replace the EXTENDED_SAMPLING compilation macro in instcode/Makefile with the NO_SAMPLING_MODE compilation macro. Assuming the source code for PEBIL for MultiMAPS tracing is under /home/userid/mmaps user needs to;
$ cd /home/userid/mmaps/PEBIL
$ vi instcode/Makefile
### change extended sampling mode to no sampling mode
#EXTRA_CFLGS = -DEXTENDED_SAMPLING -DPER_SET_RECENT -DVICTIM_CACHE
EXTRA_CFLGS = -DNO_SAMPLING_MODE -DPER_SET_RECENT -DVICTIM_CACHE
$ gmake
2.2.1.3	Using a Different Set of Memory Hierarchies (Caches)
The PEBIL source is distributed with a set of memory hierarchies to be used in cache simulation runs. However, it also is flexible enough to easily allow a different set of memory hierarchies with the installation.  PEBIL provides a PERL script under the scripts directory of the distribution, scripts/generateCaches.pl to do this.
This script takes an input file of memory hierarchy specifications for a set of systems and generates a C header file that contains these same specifications as C code. The user can also specify the stride system  to use from one of the caches given in the input file if stride information will be collected. Otherwise, the default action of the script is to add a 2MB direct cache that is used for gathering stride information. Information on how to use the generateCaches.pl script can be seen by using the --help flag. The following example shows the --help flag:
$ /home/userid /PEBIL/scripts/generateCaches.pl --help
   usage : scripts/generateCaches.pl
   [--sys-file <cache_desc_file>]  <-- defaults to CacheDescriptions.txt
   [--out-file <outfile>]                   <-- defaults to CacheStructures.h
   [--stride-sys <system idx>]       <-- defaults to 0, 2 MB direct cache
   [--nostd]                                    ? static allocation of cache structures and use of safe lib functions
   [--puniq]                                    ? print unique cache information.
   [--help]
   [--readme]
A sample input to generateCaches.pl script is given in the text box below. Everything after # sign is assumed to be a comment and is ignored. Each line in the input file defines a memory hierarchy by listing the sysid, number of cache levels and the specifications of each cache. The sysid needs to be greater than 0 and unique. It is used by the prediction database to deferentiate different systems and cache structures. Each cache is defined by 4 attributes: the cache size (which can be given in bytes or in KB or MBs), the associativity, the line size in bytes and the replacement policy.  The replacement policy can be lru, lru_vc, dir or ran where lru is the LRU pseudo implementation, lru_vc is the LRU pseudo implementation for victim caches, dir is the direct addressed and ran is a specific random replacement.
Note that each cache specification in the memory hierarchy description file needs to be per computation unit (core or processors). For instance, if L1 is private but L2 is shared between cores or processors, the L2 specification needs to be given per core/processor. Since there is no easy way of dividing the shared caches per computation unit, the simplest way is to divide caches evenly among the sharing units.
 
The output of this script is a C header file that will be compiled into the shared libraries under the instcode directory for the cache simulator rewriting tools. So if the user wants to use different caches structures or memory hierarchies for application and MultiMAPS tracing than the set of hierarchies distributed with the source (very likely), then before installing PEBIL as described in Sections 2.2.1.1 and 2.2.1.2 they need to create memory hierarchy specifications and generate the C header file for those specifications using scripts/generateCaches.pl.
For instance if the user wants to install new cache structures listed in a file named MyCaches.txt, they would perform the following steps
$ cd /home/userid /PEBIL/instcode
$ vi MyCaches,txt
  ### add your cache specifications to this file such as 
  # id level size  assoc  line  repl  size  assoc line repl  size  assoc line   repl
  1   2    256KB   8    128  lru   4MB     8    128  lru_vc
$ ../scripts/generateCaches.pl --sys-file ./MyCaches.txt
   Processing input file ---- MyCaches.txt
   Output file will be   ---- CacheStructures.h
  *** DONE *** SUCCESS *** SUCCESS *** SUCCESS *****************
$ gmake clean
$ gmake
Note that this script generates a file named CacheStructures.h by default since CacheStructures.h is the name of the file used in the source code of the instrumentation libraries. If you decide to use some other file as output, please copy that file to CacheStructures.h in the instcode directory before building the code in the instcode directory again. More importantly, since cache structures are embedded into the PEBIL installation as source code, for each different set of cache structures, a new installation of PEBIL is required. 


2.4	Tracing with PEBIL or PMaCInstrumentor
The computational traces for an application run are gathered with the aid of binary instrumentation. During application tracing, a trace file for each task in the application is gathered. Such a trace file includes cache hit rates across the specified set of cache structures for each basic block that was executed by that task during the application run. Since it is not practical to perform cache simulation on all basic blocks in the application (there might be hundreds of thousands), the computational traces are collected in two steps. During the first step, the executable is instrumented to count the number executions for each basic block (JBB Tracing). During the second step, only the most frequently executed basic blocks are chosen to be instrumented with cache simulation code (Cache Simulation). This means that to gather the computational traces for an application, it needs to be run twice using two different instrumented binaries. For the complete computational trace, all of the trace files generated during these two steps need to be used by the PMaC Prediction Framework.
The computational traces for an application are gathered by instrumenting the application binary using either PMaCInstrumentor or PEBIL. To help and ease the gathering of computational traces, these tools each provide a PERL script called traceHelper.pl under the scripts directory (and under bin directory also if the installation steps are followed). This script is designed to guide the user to instrument executables and to collect the computational traces that are used to generate performance predictions. The following two subsections describe the steps of computational trace collection in details.
For the remainder of this section we will assume that pmacInst is installed at /site/pmac_tools/Instrumentor, the application to be instrumented is compiled at /home/userid/app and the name of the executable is app.exe that is under the same directory, and the executable is run from the /home/userid/run directory. 
For pmacinst or pebil to be used for computation tracing, it inserts calls to shared libraries from the lib directory of the installation into the executable to dump the traces to the disk in addition to inserting the cache simulation code. The traceHelper.pl script allows the user to specify the path to those shared libraries using --inst_lib option every time it is executed . The value for the --inst_lib option should point to the top directory of PMaCInstrumentor/PEBIL installation, not the lib directory under the installation.
2.4.1	JBB Tracing
JBB Tracing instruments the executable to insert a counter at every basic block. Thus during the execution of the instrumented executable, it counts the number of times each basic block is executed. For JBB tracing, user needs to instrument the executable using traceHelper.pl and run the executable in the same fashion as original. To instrument the executable for JBB tracing, the user can run the following:
$ /site/pmac_tools/Instrumentor/scripts/traceHelper.pl 
                        --action jbbinst
                        --application app --dataset standard --cpu_count  64 
                        --exec_file /home/userid/app/app.exe
                        --pmacinst_dir $HOME/mytraces
                        --inst_lib /site/pmac_tools/PMaCInstrumentor
If successful, the traceHelper.pl script will create a directory, named pmacTRACE, under $HOME/mytraces. All tracing-related files that are generated will be copied to this directory. Under this directory, using the action type given by --action option, traceHelper.pl will create subdirectories and save the tracing related files including the instrumented executable and the static files that are needed later for post-processing the traces. Here is an example layout of the directories after this step.
$ ls $HOME/mytraces/pmacTRACE/
   jbbinst
$ ls $HOME/mytraces/pmacTRACE/jbbinst/
   app_standard_0064
$ ls $HOME/mytraces/pmacTRACE/jbbinst/app_standard_0064/
   App.exe.jbbinst         app.exe.jbbinst.static 
Note that the --exec_file option points to the path to the executable file and the --pmacinst_dir option points to the top directory where all traces will be collected.
The app.exe.jbbinst file under $HOME/mytraces/pmacTRACE/jbbinst/app_standard_0064/ directory is the instrumented executable and user needs to run this executable the same as the original to generate basic block execution counts. If run successfully, it will generate trace files in the run directory as follows (assuming the run directory is /home/userid/run). The number of trace files generated should match the number of tasks in the application run. Furthermore, the user should expect the execution of the jbb-instrumented application to take a factor of 1.5-2.0 times longer than the original application run.
$ ls /home/userid/run/*.meta*.jbbinst
  app.exe.meta_0000.jbbinst
  app.exe.meta_0001.jbbinst
  .............................................
  App.exe.meta_0064.jbbinst
If the trace files do not include the rank id as part of name as above (0000,0001, etc.) but include some other non-consecutive numbers (process IDs), it indicates that PMaC timers are not linked in to the executable properly. Make sure the times are inserted properly. Similarly, if the instrumented run does not generate the trace files, that indicates the termination of run was not recognized by the instrumentation code and the timers need to be included. In both cases, the RankPid file in the run directory is not expected to exist, indicating that the timers are not included properly. 
The user needs to collect these trace files under the mytraces directory by running the following command.
$ /site/pmac_tools/Instrumentor/scripts/traceHelper.pl 
                        --action jbbcoll
                        --application app --dataset standard --cpu_count  64 
                        --exec_name app.exe
                        --jbb_trc_dir /home/userid/run
                        --pmacinst_dir $HOME/mytraces
If the script runs successfully, it will generate a subdirectory under $HOME/mytraces/pmacTRACE with the action name. Under that directory, there will be an application-specific directory to where the script will copy the trace files.
$ ls $HOME/mytraces/pmacTRACE/
  jbbcoll  jbbinst
$ ls $HOME/mytraces/pmacTRACE/jbbcoll/
  app_standard_0064
$ ls $HOME/mytraces/pmacTRACE/jbbcoll/app_standard_0064/
  app.exe.meta_0000.jbbinst
  app.exe.meta_0001.jbbinst
  ......................................
  app.exe.meta_0064.jbbinst
Note that rather than a path to the executable, this command takes the executable name using the --exec_name option. Note also that the --jbb_trc_dir points to the directory where the *.meta*.jbbinst trace files are located.
2.4.2	Cache Simulation
The JBB tracing generates files that contain basic block execution counts. Since it is not practical to instrument all basic blocks for cache simulation, PMaC framework chooses a subset of the most frequently executed basic blocks that cover only a certain percentile of all basic block execution counts. The user does not need to know anything about how they are chosen as the traceHelper.pl script automatically calls the selectSimBlocks.pl script to choose the blocks to instrument for cache simulation. The next steps for computational trace collection is to instrument the executable for cache simulation, run the instrumented executable and then collect the cache simulation traces generated. 
The user needs to ensure that the following are true before continuing with this step; First, the selectSimBlocks.pl script is his path (please refer to Section 2.1.1.1). Second, the target cache structures and memory hierarchies are installed for cache simulation rather than the sample structures shipped by default with the distribution (please refer to Section 2.1.1.3).
First, the user needs to instrument the executable for cache simulation. This can be done by running the following command.
$ /site/pmac_tools/Instrumentor/scripts/traceHelper.pl 
        --action siminst
        --application app --dataset standard --cpu_count  64 
        --exec_file /home/userid/app/app.exe
        --jbb_trc_dir $HOME/mytraces/pmacTRACE/jbbcoll/app_standard_0064/
        --jbb_static $HOME/mytraces/pmacTRACE/jbbinst/app_standard_0064/app.exe.jbbinst.static 
        --phase_no 1 --phase_count 1
        --pmacinst_dir $HOME/mytraces
        --inst_lib /site/pmac_tools/Instrumentor
If successful, this command will instrument the executable with cache simulation code and save the generated files under the $HOME/mytraces directory as follows.
$ ls $HOME/mytraces/pmacTRACE/
  jbbcoll  jbbinst  siminst
$ ls $HOME/mytraces/pmacTRACE/siminst/
app_standard_0064
$ ls $HOME/mytraces/pmacTRACE/siminst/app_standard_0064/
p01
$ ls $HOME/mytraces/pmacTRACE/siminst/app_standard_0064/p01/
  app.phase.1o1.0064.jbbinst.lbb         
  app.exe.phase.1.0064.siminst         
  app.exe.phase.1.0064.siminst.static
The app.exe.phase.1.0064.siminst file is the instrumented executable for cache simulations. The user needs to run this executable the same way as the original executable to generate traces for cache simulation. Note that unlike JBB tracing, to instrument the executable for cache simulation, we also passed number of phases (using the --phase_count option) and phase number (using the --phase_no option) to the helper script. This is due to fact that cache simulation is computationally expensive and may introduce slowdowns of 6 to 10-fold during the run of the instrumented executable. In the HPC resource being used to run the instrumented executable, the upper limit in execution imposed by the system may not be large enough to complete tracing of the application in full. So users can divide the cache simulation into multiple phases, instrument the executable to perform cache simulation for only those blocks selected for a particular phase, then run each phase's instrumented executable. Note that the pmacTRACE directory structures uses the phase number for saving the cache simulation files generated (but most of the tracing we have conducted up to now has required only 1 phase).
If the user runs the instrumented executable app.exe.phase.1.0064.siminst in a manner similar to the original executable and if the run completes successfully, it should generate trace files as follows. The number of trace files should match the number of tasks in the application run.
$ ls /home/userid/run/*phase*.meta*.siminst
app.exe.phase.1.meta_0000.0064.siminst
app.exe.phase.1.meta_0001.0064.siminst
...................................................................
app.exe.phase.1.meta_0063.0064.siminst
If the trace files do not include the rank id as part of name as above (0000,0001, and son on) but include some other non-consecutive numbers (process IDs), it indicates that PMaC timers are not linked in to the executable properly as described in Section4.1 . Make sure the times are inserted properly. Similarly, if the instrumented run does not generate the trace files, that indicates the termination of run was not recognized by the instrumentation code and the timers need to be included. In both cases, the RankPid file in the run directory is not expected to exist, indicating that the timers are not included properly. 

After running the instrumented executable, the user needs to collect these traces using the simcoll action (which is functionally similar to what the jbbcoll action does for jbb tracing).
$ /site/pmac_tools/Instrumentor/scripts/traceHelper.pl 
                        --action simcoll
                        --application app --dataset standard --cpu_count  64 
                        --exec_name app.exe
                        --sim_trc_dir /home/userid/run
                        --pmacinst_dir $HOME/mytraces
                        --phase_no 1
Note that the --phase_no option is passed to the helper script to indicate which phase's traces are being collected under the $HOME/mytraces/pmacTRACE directory. If ran successfully, the trace files will be copied and the directory structure will be.
$ ls $HOME/mytraces/pmacTRACE/
  jbbcoll  jbbinst  simcoll  siminst
$ ls $HOME/mytraces/pmacTRACE/simcoll/
  app_standard_0064
$ ls $HOME/mytraces/pmacTRACE/simcoll/app_standard_0064/
  p01
$ ls $HOME/mytraces/pmacTRACE/simcoll/app_standard_0064/p01/
  app.exe.phase.1.meta_0000.0064.siminst
  app.exe.phase.1.meta_0001.0064.siminst
  ......................................................................
  app.exe.phase.1.meta_0063.0064.siminst
After this step, all traces for the application for a particular dataset, size and CPU count should be complete. For each application or dataset/size/CPU count that will be traced for performance prediction, similar steps are required. The same directory for the --pmacinst_dir option can be used  for the other applications, datasets, sizes and CPU counts to collect all trace files conveniently under one roof even though this is not a requirement. 



