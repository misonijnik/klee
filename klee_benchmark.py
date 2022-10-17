#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys
import shutil
import time

class TestRunner(object):

    def __init__(self, source_dir, timeout, output, build, llvm):
        self.cwd = os.getcwd()

        if os.path.isabs(source_dir):
            self.source_directory = source_dir
        else:
            self.source_directory = os.path.join(self.cwd, source_dir)

        if not os.path.isdir(self.source_directory):
            print("The given source directory path is malformed or does not exist.")
            print(f"Given source dir: {self.source_directory}")
            sys.exit(1)


        if llvm:
            if os.path.isabs(llvm):
                self.llvm = llvm
            else:
                self.llvm = os.path.join(self.cwd, llvm)

                if not os.path.isdir(self.llvm):
                    print("The given llvm toolchain directory path is malformed or does not exist.")
                    print(f"Given llvm dir: {self.llvm}")
                    sys.exit(1)

        else:
            self.llvm = None



        if os.path.isabs(build):
            self.build = build
        else:
            self.build = os.path.join(self.cwd, build)

        if not os.path.isdir(self.build):
            print("The given klee build directory path is malformed or does not exist.")
            print(f"klee build dir: {self.build}")
            sys.exit(1)

        self.klee_path = os.path.join(self.build, "bin", "klee")
        # build directory should be just inside the klee base directory
        self.include_path = os.path.join(self.build, "..", "include")
        self.lib_path = os.path.join(self.build, 'lib')

        if os.path.isabs(output):
            self.output_dir = output
        else:
            self.output_dir = os.path.join(self.cwd, output)

        if os.path.exists(self.output_dir):
            print("Coverage output directory already exists. To continue, move or remove the directory.")
            print(f"Coverage output dir: {self.output_dir}")
            sys.exit(1)

        try:
            os.mkdir(self.output_dir)
        except OSError:
            print("Error during coverage output directory creation.")
            sys.exit(1)

        self.klee_output_dir = os.path.join(self.output_dir, "__klee_output")

        try:
            os.mkdir(self.klee_output_dir)
        except OSError:
            print("Error during klee output directory creation.")
            sys.exit(1)

        self.timeout = timeout
        self.solverTimeout = 5

    def compile(self, source):
        print(f"Compiling {source}")
        path_to_clang = os.path.join(self.llvm,"bin","clang") if self.llvm else "clang"
        path_to_llvm_link = os.path.join(self.llvm,"bin","llvm-link") if self.llvm else "llvm-link"
        instance_dir = os.path.join(self.output_dir, os.path.splitext(source)[0])
        os.mkdir(instance_dir)
        source = shutil.copy(os.path.join(self.source_directory, source), instance_dir)

        compiler_options = ["-O0", "-Xclang", "-disable-O0-optnone"]
        compiled_file = os.path.join(instance_dir, os.path.basename(source) + '.bc')
        cmd = [path_to_clang, "-I", self.include_path, "-c", "-Wno-everything",  "-g", "-emit-llvm", "-o", compiled_file, source] + compiler_options
        subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        # Compile and link TestComp functions definitions
        compiled_library =  os.path.join(instance_dir, "klee-test-comp.bc")
        test_comp_source = os.path.join(self.include_path, "klee-test-comp.c")
        cmd = [path_to_clang, "-c", "-g", "-emit-llvm", "-o", compiled_library, test_comp_source] + compiler_options
        subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        cmd = [path_to_llvm_link, "-o", compiled_file]
        cmd += [compiled_library, compiled_file]
        subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def run(self, source):
        print(f"Running KLEE on {source}")
        instance_dir = os.path.join(self.output_dir, os.path.splitext(source)[0])
        compiled_file = os.path.join(instance_dir, os.path.basename(source) + '.bc')
        # Add different options for KLEE
        cmd = [self.klee_path]

        # Add common arguments
        cmd += ["--output-dir=" + os.path.join(self.klee_output_dir, os.path.splitext(source)[0])]
        cmd += ["--max-time=" + str(self.timeout) + "s"]
        cmd += ["--max-solver-time=" + str(self.solverTimeout) + "s"]
        # cmd += ["--libc=uclibc"]
        # cmd += ["--posix-runtime"]
        cmd += [compiled_file]
        try:
            subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except:
            print(f"{source} failed.")

    def compile_coverage(self, source):
        print(f"Compiling {source} for coverage")
        os.environ['LD_LIBRARY_PATH'] += f":{self.lib_path}"
        instance_dir = os.path.join(self.output_dir, os.path.splitext(source)[0])
        compiled_file = os.path.join(instance_dir, os.path.basename(source) + ".o")
        source = os.path.join(instance_dir, source)
        cmd = ["gcc", "-O0", "-I", self.include_path, "-L", self.lib_path]
        cmd += ['-coverage', '-fprofile-abs-path']
        cmd += ["-o", compiled_file, source]
        cmd += ['-lkleeRuntest']
        subprocess.check_call(cmd, env=os.environ, cwd=instance_dir, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def run_coverage(self, source):
        print(f"Running {source} on generated tests")
        t_end = time.time() + 60
        instance_dir = os.path.join(self.output_dir, os.path.splitext(source)[0])
        compiled_file = os.path.join(instance_dir, os.path.basename(source) + ".o")
        for filename in os.listdir(os.path.join(self.klee_output_dir, os.path.splitext(source)[0])):
            if time.time() > t_end:
                print(f"Stopping running generated tests after 60 seconds, some tests have not run.")
                break
            if filename.endswith(".ktestjson"):
                os.environ["KTEST_FILE"] = os.path.join(self.klee_output_dir, os.path.splitext(source)[0] ,filename)
                os.environ['LD_LIBRARY_PATH'] += f":{self.lib_path}"
                try:
                    subprocess.run([compiled_file], env=os.environ, cwd=instance_dir, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=3)
                except subprocess.TimeoutExpired:
                    pass

    def save_gcov(self):
        subprocess.check_call(["gcovr", "-r", self.output_dir, "--html-details" ,"results"], cwd=self.output_dir, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", help='source directory')
    parser.add_argument("--t", default=30, help='timeout in seconds, default: 30')
    parser.add_argument("--output", required=True, help="where to keep coverage output, required")
    parser.add_argument("--build", required=True, help="klee build directory, required")
    parser.add_argument("--llvm", default=None, help="directory of the llvm toolchain to use for compilation, default: system toolchain")
    parser.add_argument("--just-compile", choices=('True','False'), help="Just compile the files")
    args = parser.parse_args()

    wrapper = TestRunner(args.source, args.t, args.output, args.build, args.llvm)
    count = 1;
    num_sources = len([name for name in os.listdir(args.source)])
    print(f"{num_sources} total.")
    for filename in os.listdir(args.source):
        if args.just_compile == 'True':
            wrapper.compile(filename)
        else:
            print(f"Source {count}/{num_sources}")
            wrapper.compile(filename)
            wrapper.run(filename)
            wrapper.compile_coverage(filename)
            wrapper.run_coverage(filename)
            count += 1
    wrapper.save_gcov()

if __name__ == '__main__':
    main()
