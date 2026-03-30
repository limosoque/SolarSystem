#include "Game.h"
#include "SolarSystemComponent.h"

int main()
{
    Game game(L"Solar System", 1920, 1080);

    auto* solarSystem = new SolarSystemComponent(&game);
    game.Components.push_back(solarSystem);

    game.Initialize();
    game.Run();

    return 0;
}