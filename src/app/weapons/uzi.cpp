#include "uzi.h"

Uzi::Uzi() : Gun(0, 1, 50, 0.1, 10) {

}

QString Uzi::name() const {
    return "Uzi";
}
