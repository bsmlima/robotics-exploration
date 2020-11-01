#include "Planning.h"

#include <queue>
#include <float.h> //DBL_MAX
#include <GL/glut.h>

////////////////////////
///                  ///
/// Métodos Públicos ///
///                  ///
////////////////////////

Planning::Planning()
{
    newRobotPosition.x = 0;
    newRobotPosition.y = 0;

    robotPosition = newRobotPosition;

    newGridLimits.minX = newGridLimits.minY = 1000;
    newGridLimits.maxX = newGridLimits.maxY = -1000;

    gridLimits = newGridLimits;
}

Planning::~Planning()
{}

void Planning::setGrid(Grid *g)
{
    grid = g;
}

void Planning::setMaxUpdateRange(int r)
{
    maxUpdateRange = 1.2*r*grid->getMapScale();
}

void Planning::setNewRobotPose(Pose p)
{
    newRobotPosition.x = (int)(p.x*grid->getMapScale());
    newRobotPosition.y = (int)(p.y*grid->getMapScale());

    newGridLimits.minX = std::min(newGridLimits.minX,newRobotPosition.x-maxUpdateRange);
    newGridLimits.maxX = std::max(newGridLimits.maxX,newRobotPosition.x+maxUpdateRange);
    newGridLimits.minY = std::min(newGridLimits.minY,newRobotPosition.y-maxUpdateRange);
    newGridLimits.maxY = std::max(newGridLimits.maxY,newRobotPosition.y+maxUpdateRange);
}

void Planning::run()
{
    pthread_mutex_lock(grid->mutex);

    resetCellsTypes();

    // update robot position and grid limits using last position informed by the robot
    robotPosition = newRobotPosition;
    gridLimits = newGridLimits;

    updateCellsTypes();

    pthread_mutex_unlock(grid->mutex);

    initializePotentials();

    for(int i=0; i<100; i++){
        iteratePotentials();
    }

    updateGradient();
}

/////////////////////////////////////////////
///                                       ///
/// Métodos para classificacao de celulas ///
///                                       ///
/////////////////////////////////////////////

void Planning::resetCellsTypes()
{
    for(int i=gridLimits.minX;i<=gridLimits.maxX;i++){
        for(int j=gridLimits.minY;j<=gridLimits.maxY;j++){

            Cell* c = grid->getCell(i,j);
            c->planType = REGULAR;
        }
    }
}

void Planning::updateCellsTypes()
{
    Cell* c;

    // If you want to access all observed cells (since the start), use this range
    //
    //  (gridLimits.minX, gridLimits.maxY)  -------  (gridLimits.maxX, gridLimits.maxY)
    //                  |                     \                      |
    //                  |                      \                     |
    //                  |                       \                    |
    //  (gridLimits.minX, gridLimits.minY)  -------  (gridLimits.maxX, gridLimits.minY)

    // TODO: classify cells

    // the occupancy type of a cell can be defined as:
    // c->occType = UNEXPLORED
    // c->occType = OCCUPIED
    // c->occType = FREE

    // the planning type of a cell can be defined as:
    // c->planType = REGULAR
    // c->planType = FRONTIER
    // c->planType = DANGER
    // c->planType = NEAR_WALLS
    // c->planType = FRONTIER_NEAR_WALL


    for (int cellX = gridLimits.minX; cellX <= gridLimits.maxX; cellX++) {
        for (int cellY = gridLimits.minY; cellY <= gridLimits.maxY; cellY++) {
            Cell *cell = grid->getCell(cellX, cellY);

            if (cell->himm <= 5) cell->occType = FREE;
            if (cell->himm >= 10) cell->occType = OCCUPIED;
        }
    }

    
    for (int cellX = gridLimits.minX; cellX <= gridLimits.maxX; cellX++) {
        for (int cellY = gridLimits.minY; cellY <= gridLimits.maxY; cellY++) {
            Cell *cell = grid->getCell(cellX, cellY);

            cell->planType = REGULAR;

            if (cell->occType == FREE) {
                for (int x = cellX - 8; x <= cellX + 8; x++) {
                    for (int y = cellY - 8; y <= cellY + 8; y++) {
                        Cell *adjacentCell = grid->getCell(x, y);

                        if (adjacentCell->occType == OCCUPIED)
                            cell->planType = NEAR_WALLS;
                    }
                }
            }

            if (cell->occType == FREE) {
                for (int x = cellX - 3; x <= cellX + 3; x++) {
                    for (int y = cellY - 3; y <= cellY + 3; y++) {
                        Cell *adjacentCell = grid->getCell(x, y);

                        if (adjacentCell->occType == OCCUPIED)
                            cell->planType = DANGER;
                    }
                }
            }
        }
    }

    for (int cellX = gridLimits.minX; cellX <= gridLimits.maxX; cellX++) {
        for (int cellY = gridLimits.minY; cellY <= gridLimits.maxY; cellY++) {
            Cell *cell = grid->getCell(cellX, cellY);

            if (cell->occType == UNEXPLORED) {
                for (int x = cellX - 1; x <= cellX + 1; x++) {
                    for (int y = cellY - 1; y <= cellY + 1; y++) {
                        Cell *adjacentCell = grid->getCell(x, y);

                        if (adjacentCell->occType == FREE)
                            cell->planType = FRONTIER;

                    }
                }

                for (int x = cellX - 1; x <= cellX + 1; x++) {
                    for (int y = cellY - 1; y <= cellY + 1; y++) {
                        Cell *adjacentCell = grid->getCell(x, y);

                        if (adjacentCell->planType == DANGER || adjacentCell->planType == NEAR_WALLS)
                            cell->planType = FRONTIER_NEAR_WALL;
                    }
                }
            }
        }
    }

}

void Planning::initializePotentials()
{
    Cell *c;

    // the potential of a cell is stored in:
    // c->pot[i]
    // the preference of a cell is stored in:
    // c->pref

    // TODO: initialize the potential field in the known map
    //
    //  (gridLimits.minX, gridLimits.maxY)  -------  (gridLimits.maxX, gridLimits.maxY)
    //                  |                     \                      |
    //                  |                      \                     |
    //                  |                       \                    |
    //  (gridLimits.minX, gridLimits.minY)  -------  (gridLimits.maxX, gridLimits.minY)




}

void Planning::iteratePotentials()
{
    Cell* c;
    Cell *left,*right,*up,*down;

    // the update of a FREE cell in position (i,j) will use the potential of the four adjacent cells
    // where, for example:
    //     left  = grid->getCell(i-1,j);


    // TODO: iterate the potential field in the known map
    //
    //  (gridLimits.minX, gridLimits.maxY)  -------  (gridLimits.maxX, gridLimits.maxY)
    //                  |                     \                      |
    //                  |                      \                     |
    //                  |                       \                    |
    //  (gridLimits.minX, gridLimits.minY)  -------  (gridLimits.maxX, gridLimits.minY)





}

void Planning::updateGradient()
{
    Cell* c;

    // the components of the descent gradient of a cell are stored in:
    // c->dirX[i] and c->dirY[i], for pot[i]

    Cell *left,*right,*up,*down;

    // the gradient of a FREE cell in position (i,j) is computed using the potential of the four adjacent cells
    // where, for example:
    //     left  = grid->getCell(i-1,j);


    // TODO: compute the gradient of the FREE cells in the known map
    //
    //  (gridLimits.minX, gridLimits.maxY)  -------  (gridLimits.maxX, gridLimits.maxY)
    //                  |                     \                      |
    //                  |                      \                     |
    //                  |                       \                    |
    //  (gridLimits.minX, gridLimits.minY)  -------  (gridLimits.maxX, gridLimits.minY)







}

