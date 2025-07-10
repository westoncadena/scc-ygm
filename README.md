# Strongly Connected Components (SCC) with YGM
  
This project implements a distributed algorithm for finding **Strongly Connected Components (SCC)** in large graphs using [YGM (You Got Mail)](https://github.com/LLNL/ygm), a C++ library for message-driven distributed computing.

## Reference

This implementation is based on the algorithm described in the paper:

> **Distributed Strongly Connected Components via Message Passing**
> [ACM Digital Library Link](https://dl.acm.org/doi/10.1145/3581784.3607071)

## Features

- Distributed SCC computation for large-scale graphs
- Utilizes YGM for efficient message passing
- Scalable to many nodes

## Getting Started

### Prerequisites

- C++20 or newer
- MPI (e.g., OpenMPI or MPICH)
- [YGM library](https://github.com/LLNL/ygm)

## Building Your Code
After your executable has been added to the CMake setup, it can be built by running
```
# If on Ruby
module load cmake/3.30.5 gcc/13.3.1

############
mkdir build
cd build
cmake ..
make
```

## VS Code Users
By default, VS Code's C++ IntelliSense does not know the location YGM's `include` directories will be at from our CMake
configuration. This means code completion features will not work without some modification. To remedy this:
1. Open the `C/C++ Configuration` in VS Code (`C/C++: Edit Configuration (UI)` in the Command Palette).
2. Navigate to `Compile commands` in `Advanced Settings`.
3. Add `${workspaceFolder}/build/compile_commands.json` to the dialog box.

After making this change one time, code completion features for YGM will work in any project built from this template.

## Acknowledgments

- [YGM library](https://github.com/LLNL/ygm)
- [Distributed Strongly Connected Components via Message Passing](https://dl.acm.org/doi/10.1145/3581784.3607071)



