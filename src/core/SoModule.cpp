#include <iostream>

#include "src/core/SoModule.hpp"

SoModule::SoModule () {}
SoModule::SoModule (bool equal) : equal (equal) {}

bool SoModule::operator== (const SoModule& a) const { return mod == a.mod; }
bool SoModule::Run (Privdata&) const { return true; }
int SoModule::Status (Privdata &) const { return 0; }
void SoModule::InitMsg (Privdata &) const { }
void SoModule::InitRcpt (Privdata&) const { }
void SoModule::DestroyMsg (Privdata&) const { }
void SoModule::DestroyRcpt (Privdata &) const { }
bool SoModule::Equal (Privdata &, const Recipient&, const Recipient&) const {
  return equal;
}
SoModule::~SoModule () {}
