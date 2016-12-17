import inspect
import sys
import os
import asyncio
import importlib


async def run_module(gf, name):
    cwd = os.path.abspath(os.getcwd())
    sys.path.append(os.path.join(cwd, 'tests'))
    mod = importlib.import_module(name, package='tests')

    test_classes = []
    for name, obj in inspect.getmembers(mod):

        if inspect.isclass(obj) and issubclass(obj, TestCase) and obj is not TestCase:
            test_classes.append(obj)

    for test_class in test_classes:
        test = test_class(gf)
        test_methods = {}

        for name, obj in inspect.getmembers(test):
            if inspect.ismethod(obj) and name.startswith('test_'):
                test_methods[name] = obj

        await test.setup()

        futures = set()
        names = {}
        for name, case in test_methods.items():
            future = asyncio.ensure_future(case())
            futures.add(future)
            names[future] = name

        while futures:
            done, _ = await asyncio.wait(futures, return_when=asyncio.FIRST_COMPLETED)
            futures -= done
            for future in done:
                status = 'unknown'
                message = None

                try:
                    future.result()

                except AssertionError as e:
                    status = 'fail'
                    message = str(e)

                except Exception as e:
                    status = 'error'
                    message = str(e)

                else:
                    status = 'pass'

                print('%s ... %s' % (names[future], status))

                if message is not None:
                    print(message)

        await test.teardown()


class TestCase:
    def __init__(self, dut):
        self.dut = dut

    def setup(self):
        pass

    def teardown(self):
        pass
