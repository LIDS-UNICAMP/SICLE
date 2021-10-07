# Felipe Belém's PhD Library

This project contains all the algorithms and programs used and developed by [Felipe Belém](https://ic.unicamp.br/~felipe.belem) during his PhD studies at the University of Campinas (UNICAMP), with the cooperation of the Pontifical Catholic University of Minas Gerais (PUC-MG), both held in Brazil. During this time, Felipe integrated both  [Laboratory of Image and Data Science](http://lids.ic.unicamp.br/) (LIDS) and the [Image and Multimedia Data Science Laboratory](http://imscience.icei.pucminas.br/) (IMSCIENCE) as a researcher.

### 1.  Setup

The project was developed in **C** under a **Linux-based** operational system; therefore, it is **NOT GUARANTEED** to work properly in other systems (_e.g._ Windows and macOS). Moreover, the same applies for **non-GCC** compilers, such as Clang and MinGW.

All code within this project were developed, compiled and tested using the following programs:
- **[GNU GCC](https://gcc.gnu.org/)**: version 7.5.0
- **[GNU Make](https://www.gnu.org/software/make/)**: version 4.1

The library has in-built support for handling **PNM** and **SCN** images as single files or video folders. For enabling external library support, please refer to the [README](externals/README.md) file within the **externals** folder.

### 2.  Compiling

If your computer meets the aforementioned requirements, you may run the commands below for compiling the library and all the demonstration programs.
```bash
make lib
make demo
```
Or simply run one of the following commands for compiling both latter at once.
```bash
make
make all
```

For removing the generated files, the following rules are provided.
```bash
make tidy
make clean
```
The first one only deletes the binary files generated from compiling the demonstration programs; whereas the second goes beyond and deletes the object files and the library file as well.

For convenience during implementation, the rule presented below offers a simple and easy approach for recompiling all library (but not the demonstration programs) from scratch.
```bash
make refresh
```

### 3. Documentation

For each program within the **demo** folder, there is a more detailed description, in the **doc** folder, of its usage and its expected behavior. Still, the source code and its headers are also extensively documented with commentaries for facilitating the programmer's understanding of the code.

### 4. License

All codes within this project are under the **MIT License**. See the [LICENSE](LICENSE) file for more details.

### 5. Acknowledgements

This project was financially supported by numerous brazilian research funding agencies: 
- Conselho Nacional de Desenvolvimento Científico e Tecnológico (CNPq)
- Coordenação de Aperfeiçoamento de Pessoal de Nível Superior (CAPES)
- Fundação de Amparo à Pesquisa do Estado de Minas Gerais (FAPEMIG)
- Fundação de Amparo à Pesquisa do Estado de São Paulo (FAPESP)

### 6. Contact

If you have any questions or faced an unexpected behavior (_e.g._ bugs), please feel free to contact the author or his advisors through the following email addresses:
- **Felipe C. Belém**:  [felipe.belem@ic.unicamp.br](mailto:felipe.belem@ic.unicamp.br)
- **Silvio Jamil F. Guimarães**:  [sjamil@pucminas.br](mailto:sjamil@pucminas.br)
- **Alexandre X. Falcão**: [afalcao@ic.unicamp.br](mailto:afalcao@ic.unicamp.br)
