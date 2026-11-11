// expected-error: conversion|convert
#include <uarch/detect/detect.hpp>
uarch::arch::Tier rejected_tier = uarch::detect::classify(uarch::detect::Snapshot{});
uarch::arch::X86V3 rejected_token = uarch::detect::classify(uarch::detect::Snapshot{});
