# This file holds the CPack configuration

###############################################################################
# Generalities
###############################################################################

set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_NAME "eovim")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Eovim is the Enlightened Neovim")
set(CPACK_PACKAGE_VENDOR "Jean Guyomarc'h")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_FILE_NAME
   "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")


###############################################################################
# RPM Generator
###############################################################################

set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_RELOCATABLE TRUE)
set(CPACK_RPM_PACKAGE_GROUP "Applications/Editors")
set(CPACK_RPM_PACKAGE_DESCRIPTION
   "Eovim is the Enlightened Neovim. That's just an EFL GUI client for Neovim.")
set(CPACK_RPM_PACKAGE_REQUIRES
   "neovim >= 0.2.0, efl >= ${EFL_MIN_VERSION}")

include(CPack)
