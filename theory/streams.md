std::cin
Stands for: “character input”
What: Standard input stream (usually the keyboard).
How it works: Reads data from the user or input redirection.
Under the hood: cin is an object of type std::istream, connected to the standard input file descriptor (stdin). It buffers input and provides formatted extraction (>>), skipping whitespace and converting to the requested type.
=====================================================================
2. std::ifstream
Stands for: “input file stream”
What: Reads data from files.
How it works: You open a file, and then use >> or getline to read from it.
Under the hood: ifstream is derived from std::istream and manages a file buffer. It reads bytes from a file on disk, buffering them for efficient access and providing formatted extraction.
======================================================================
3. std::istringstream
Stands for: “input string stream”
What: Reads data from a string in memory, as if it were a stream.
How it works: You initialize it with a std::string, then use >> to extract words or values.
Under the hood: istringstream is derived from std::istream and uses a string buffer. It lets you parse strings using the same stream operations as cin or ifstream.
=======================================================================

Summary Table
Stream	Reads from	Typical Use
std::cin	Keyboard/stdin	User input
std::ifstream	File	Reading files
std::istringstream	std::string	Parsing strings in memory
All three use the same stream interface, so you can use >>, getline, and other operations in a consistent way, regardless of the data source.