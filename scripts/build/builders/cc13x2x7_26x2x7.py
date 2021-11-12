# Copyright (c) 2021 Project CHIP Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
from enum import Enum, auto

from .gn import GnBuilder


class TIApp(Enum):
    LOCK = auto()
    PUMP = auto()

    def ExampleName(self):
        if self == TIApp.LOCK:
            return 'lock-app'
        elif self == TIApp.PUMP:
            return 'pump-app'
        else:
            raise Exception('Unknown app type: {}'.format(self))

    def AppName(self):
        if self == TIApp.LOCK:
            return 'lock-example'
        elif self == TIApp.PUMP:
            return 'pump-example'
        else:
            raise Exception('Unknown app type: {}'.format(self))

    def FlashBundleName(self):
        if self == TIApp.LOCK:
            return 'lock_app.flashbundle.txt'
        elif self == TIApp.PUMP:
            return 'pump_app.flashbundle.txt'
        else:
            raise Exception('Unknown app type: %r' % self)

    def BuildRoot(self, root):
        return os.path.join(root, 'examples', self.ExampleName(), 'cc13x2x7_26x2x7')


class TIBoard(Enum):
    LP_CC2652R7 = 1

    def BoardName(self):
        if self == TIBoard.LP_CC2652R7:
            return 'LP_CC2652R7'


class TIBuilder(GnBuilder):

    def __init__(self,
                 root,
                 runner,
                 app: TIApp = TIApp.LOCK,
                 board: TIBoard = TIBoard.LP_CC2652R7):
        super(TIBuilder, self).__init__(
            root=app.BuildRoot(root),
            runner=runner)
        self.app = app
        self.board = board

    def generate(self):
        if 'TI_SIMPLELINK_SDK_ROOT' not in os.environ:
            raise Exception("CC13x3x7_26x2x7 builds require TI_SIMPLELINK_SDK_ROOT to be set")

        if 'TI_SYSCONFIG_ROOT' not in os.environ:
            raise Exception("CC13x3x7_26x2x7 builds require TI_SYSCONFIG_ROOT to be set")

        super().generate()

    def GnBuildArgs(self):
        return [
            'ti_simplelink_board="{}"'.format(self.board.BoardName()),
            'ti_simplelink_sdk_root="{}"'.format(os.environ['TI_SIMPLELINK_SDK_ROOT']),
            'ti_sysconfig_root="{}"'.format(os.environ['TI_SYSCONFIG_ROOT'])
        ]

    def build_outputs(self):
        name = "chip-{}-{}".format(self.board.BoardName(), self.app.AppName())
        items = {
            f'{name}.out':
                os.path.join(self.output_dir, f'{name}.out'),
            f'{name}.out.map':
                os.path.join(self.output_dir, f'{name}.out.map'),
        }

        # Figure out flash bundle files and build accordingly
        with open(os.path.join(self.output_dir, self.app.FlashBundleName())) as f:
            for line in f.readlines():
                file = line.strip()
                items[f'flashbundle/{file}'] = os.path.join(self.output_dir, file)

        return items
