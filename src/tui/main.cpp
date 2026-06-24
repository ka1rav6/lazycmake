#include "lazycmake/tui/application.hpp"

int main(int argc, char* argv[]) {
    lazycmake::tui::Application app;
    return app.run(argc, argv);
}
