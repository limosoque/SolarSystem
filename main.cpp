#include "Game.h"
#include "PongComponent.h"

int main()
{
    Game game(L"Ping-pong", 800, 600);

    auto* pong = new PongComponent(&game);
    game.Components.push_back(pong);

    game.Initialize();
    game.Run();

    return 0;
}
