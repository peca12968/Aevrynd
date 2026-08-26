#include "LocalQwen.h"
#include <iostream>
int main(){LocalQwen qwen;auto event=qwen.Generate({3,14,95,4,21});if(!event){std::cerr<<"Local Qwen event generation failed\n";return 1;}std::cout<<event->type<<"|"<<event->magnitude<<"|"<<event->duration<<"|"<<event->title<<"\n";return 0;}
