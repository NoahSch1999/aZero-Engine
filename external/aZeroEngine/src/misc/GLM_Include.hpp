#pragma once
//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES // Forces types to be aligned and padded by default
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Changes depth from opengl [-1, 1] to dx and vk [0, 1]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL 
#include <glm/gtx/norm.hpp>
#include <numbers>