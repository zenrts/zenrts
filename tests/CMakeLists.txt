# ZenRTS
# Copyright (C) 2026  Ian Torres
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License version 3
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

add_executable(test_zenrts
    test_core.cpp
    test_time.cpp
    test_utils.cpp
)

target_link_libraries(test_zenrts PRIVATE zenrts gtest_main)

add_test(NAME test_zenrts COMMAND test_zenrts)
