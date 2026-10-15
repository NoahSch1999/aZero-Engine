#include "src/Editor.hpp"
#include "misc/Maths.hpp"

int main(int argc, char* argv[])
{
	AllocConsole();
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);

	aZero::Editor::Editor editor;
	editor.Run();
	
	return 0;
}