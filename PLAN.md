Here is the first draft of the plan

- @TinyHVM is a symbolic link to the research project for combining HVM and tinygrad under unified Interaction Calculus based deep learning framework
- It contains some sloppy README.md, docs/ and resources/ documents with the design and specs
- We're going to build it from scratch with a proper software engineering workflow from the beginning now that the murky mechanisms are much clearer

0. @AGENTS.md is for you to populate with your own instructions as you go
1. git init and commit often, maintain human readable CHANGELOG.md for all changes in addition to simple commit messages
2. prep src/ with a similar layout but very minimal
3. start with tests/ and write some initial unit tests for HVM interactions
4. create wl/ with a LibraryLink paclet and tests, all the functionallity to observe low-level primitives with high-level functions
5. make a src/ with initial implementation
6. start with basic heap datastructure, trampoline, wnf stack machine, reduce mechanism, minimial number of combinators, just enough to pass initial unit tests, you can copy-paste from @TinyHVM/HVM4 literally
7. keep source files small as possible, each interaction in separate file, split functionality, try to be consice without overengineering and overoptimizing with guards etc
8. create docs/ and start describing the architecture in multiple self-contained markdown files about different pieces
9. maintain README.md
10. the wl paclet should have enough primitives to render graphs of the underlying heap structure, the THeap object should have it as a property
11. use wl produced graphs in your documentation
12. add TTensor and TUOp for constructing computational graphs supporting cpu and metal backends and whatever is needed in underlying C source code
13. start adding minimal set of tinygrad UOPs with UOP_GRAD and UOP_KERNEL
14. add minimal global passes for kernelization, memory planning, optimizations, codegen and rendering the kernels
15. make your first end-to-end test to compute a linear layer output and its gradient on both backends, with trace capability to observe heap and its graph visualization after each interaction and global pass

that's it for now