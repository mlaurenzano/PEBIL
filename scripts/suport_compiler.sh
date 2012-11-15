#!/usr/bin/env bash

prg=$0
set -e

function object-blacklist(){
    readelf -s $1 | grep FUNC |  grep -v UND | awk '{print $8}' | grep -wvi "main\|main_\|main__\|_fini\|_start\|call_gmon_start"; 
}
function print_usage(){
    if [ "$1" != "" ]; then
        echo "error: $1"
        echo ""
    fi
    echo "usage:"
    echo "    ${prg} [--mpi] [--cc|--cxx|--f90|--f77] <compiler_name> --file <list_of_functions>"
}

typ=""
comp=""
outfile=""
defs=""
while [ $# -ne 0 ]; do
    case "$1" in
        --mpi) defs="${defs} -DHAVE_MPI";    shift;;
        --cc)  typ="cc"; shift; comp="$1";   shift;;
        --cxx) typ="cxx"; shift; comp="$1";  shift;;
        --f90) typ="f90"; shift; comp="$1";  shift;;
        --f77) typ="f77"; shift; comp="$1";   shift;;
        --file) shift; outfile="$1"; shift;;
        --help) print_usage ""; exit 0;;
        *) echo "unknown option $1"; exit 1;;
    esac
done

if [ "${typ}" == "" ]; then
    print_usage "missing compiler type, use one of [--cc|--cxx|--f90|--f77]"
    exit 1
fi
if [ "${outfile}" == "" ]; then
    print_usage "missing flag --file <outfile>"
    exit 1
fi
if [ ! -f `which ${comp}` ]; then
    print_usage "not a valid compiler path: ${comp}"
    exit 1
fi

tmp_file="__pebil__tmp"
list_intermed=local.func

trivialprog_CC="#ifdef HAVE_MPI\n#include <mpi.h>\n#endif\n\
int main(int argc, char** argv){\n\
#ifdef HAVE_MPI\nMPI_Init(&argc, &argv); MPI_Finalize();\n#endif\n\
return 0; }"

trivialprog_CXX="#ifdef HAVE_MPI\n#include <mpi.h>\n#endif\n\
using namespace std;\nint main(int argc, char** argv){\n\
#ifdef HAVE_MPI\nMPI::Init(argc, argv); MPI::Finalize();\n#endif\n\
return 0; }"

trivialprog_F90="\
      program test\n\
#ifdef HAVE_MPI\n\
      include 'mpif.h'\n\
#endif\n\
      integer ierr\n\
#ifdef HAVE_MPI\n\
      call MPI_Init(ierr)\n\
      call MPI_Finalize (ierr)\n\
#endif\n\
      end"

trivialprog_F77="\
#ifdef HAVE_MPI\n\
      include 'mpif.h'\n\
      integer error\n\
      call MPI_Init ( error )\n\
      call MPI_Finalize ( error )\n\
#endif\n\
      stop\n\
      end"

prep=""
if [ "${typ}" == "cc" ]; then
    program="${trivialprog_CC}"
    ext="c"
elif [ "${typ}" == "cxx" ]; then
    program="${trivialprog_CXX}"
    ext="cxx"
elif [ "${typ}" == "f90" ]; then
    program="${trivialprog_F90}"
    ext="f90"
    prep="c"
elif [ "${typ}" == "f77" ]; then
    program="${trivialprog_F77}"
    ext="f"
    prep="c"
else
    print_usage "unknown compiler typ ${typ}"
    exit 1
fi

if [ "${prep}" != "" ]; then
    echo -e "${program}" > "${tmp_file}_${ext}.${prep}"
    gcc -E "${tmp_file}_${ext}.${prep}" > "${tmp_file}.${ext}"
else
    echo -e "${program}" > "${tmp_file}.${ext}"
fi

echo "compiling ${typ} program for blacklist using compiler ${comp}"
${comp} ${defs} -o "${tmp_file}_${ext}" "${tmp_file}.${ext}"

object-blacklist "${tmp_file}_${ext}" > ${list_intermed}
if [ -f ${outfile} ]; then
    cat ${outfile} >> ${list_intermed}
fi
sort -u "$list_intermed" | grep . > ${outfile}
rm "$list_intermed"
rm "$tmp_file"*

exit 0
