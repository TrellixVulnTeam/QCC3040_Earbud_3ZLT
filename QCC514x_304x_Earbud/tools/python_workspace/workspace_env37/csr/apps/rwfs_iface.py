############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
class RwfsIface(object):
    """Python class for interfacing with the rewriteable filesystem traps.
    """
    FILE_NONE = 0 # Indicates a non existant file index, from file_if.h
    FILE_ROOT = 1 # The root file index, from file_if.h"""

    def __init__(self, device):
        """Initialises this object for manipulating the rewriteable filesystem
           on the given device.

        Args:
            device (obj): The device to operate on.
        """
        self.device = device
        self.subsys = device.chip.apps_subsystem.p1
        self._utils = self.subsys.fw.trap_utils

    def is_enabled(self):
        """Detect whether the writeable filesystem is enabled or not.

        Returns:
            bool: True if the writeable filesystem is enabled, False otherwise.
        """

        # currently RWFS is not supported on CSRA6810X, a curator patch is required to support RWFS
        _cur = self.device.chip.curator_subsystem.core
        if "csra6810x" in self.device.name:
            return False
        else:
            def random_file_name():
                import random
                import string
                return "/rwfs/is.rwfs.enabled.test.file." + \
                 ''.join(random.choice(string.ascii_uppercase) for _ in range(10))

            test_filename = random_file_name()
            try:
                self.create(test_filename)
            except ValueError:
                return False

            self.delete(test_filename)
            return True
            
    def create(self, path):
        """Creates a file in the rewriteable filesystem with the given path.

        Args:
            path (str): The path to the file to create.
                        Must start with "/rwfs/".
        Returns:
            int: The index of the created file.
        Raises:
            ValueError: If the path is invalid, or if the file already exists.
        """
        idx = self.subsys.fw.call.FileCreate(path, len(path))
        if not idx:
            raise ValueError(("Unable to create file, path is invalid or "
                              "file already exists \"{}\"").format(path))
        return idx

    def find(self, path):
        """Find the index for the given file path.

        Args:
            path (str): The path to the file to find the index for.
        Returns:
            int: The index of the file.
        Raises:
            ValueError: If the file was not found.
        """
        found_idx = self.subsys.fw.call.FileFind(self.FILE_ROOT, path,
                                                 len(path))
        if not found_idx:
            raise ValueError("File not found \"{}\"".format(path))
        return found_idx

    def write(self, path, data):
        """Write data to a file.

        Args:
            path (str): The path to the file to write to. This file may or may
                    not already exist. If it doesn't exist it's created, if it
                    does it is appended to.
            data (list of bytes): The data to write to the file. Each byte in
                    the list should be in the range [0, 255].
        Returns:
            int: The index of the file that was written to.
        Raises:
            ValueError: If the file was unable to be created.
            ValueError: If 'data' contains integers outside the range [0, 255]

        """
        if not all(0 <= d <= 255 for d in data):
            raise ValueError("data contains values outside the range [0, 255]")

        try:
            idx = self.create(path)
        except ValueError:
            # Creation failed, could be that the file already exists or the
            # path is invalid.
            try:
                idx = self.find(path)
            except ValueError:
                # Unable to find file, path must be invalid
                raise ValueError("Invalid file path \"{}\"".format(path))

        snk = self.subsys.fw.call.StreamFileSink(idx)

        to_write = list(data)
        while to_write:
            slack = self.subsys.fw.call.SinkSlack(snk)
            length = min(slack, len(to_write))
            if (length <= 0):
                break
            mapped = self.subsys.fw.call.SinkMap(snk)
            claimed = self.subsys.fw.call.SinkClaim(snk, length)
            write_ptr = mapped + claimed
            self._utils.copy_bytes_to_device(write_ptr, to_write[:length])
            del to_write[:length]
            flush_ret = self.subsys.fw.call.SinkFlushBlocking(snk, length)

        self.subsys.fw.call.SinkClose(snk)
        return idx

    def read(self, path):
        """Read data from a file.

        Args:
            path (str): The path to the file to read from. This file must
                    already exist in the rewriteable filesystem.
        Returns:
            list of bytes: A list of the data in the file.
        Raises:
            ValueError: If the file was not found.
        """
        file_index = self.find(path)
        file_source = self.subsys.fw.call.StreamFileSource(file_index)

        read_data = []
        size = self.subsys.fw.call.SourceSizeBlocking(file_source)
        while size != 0:
            mapped_ptr = self.subsys.fw.call.SourceMap(file_source)
            read_data.extend(self.subsys.dm[mapped_ptr:mapped_ptr + size])
            self.subsys.fw.call.SourceDrop(file_source, size)
            size = self.subsys.fw.call.SourceSizeBlocking(file_source)
        self.subsys.fw.call.SourceClose(file_source)
        return read_data

    def source(self, path):
        """Create a File Source from the given file path.

        Args:
            path (str): The path to create a File Source from.
        Returns:
            int: The File Source index.
        Raises:
            ValueError: If the file was not found.
        """
        file_index = self.find(path)
        return self.subsys.fw.call.StreamFileSource(file_index)

    def sink(self, path):
        """Create a File Sink from the given file path.

        Args:
            path (str): The path to create a File Sink from.
        Returns:
            int: The File Sink index.
        Raises:
            ValueError: If the file was not found.
        """
        file_index = self.find(path)
        return self.subsys.fw.call.StreamFileSink(file_index)

    def delete(self, path):
        """Deletes a file from the rewriteable file system.

        Args:
            path (str): The path to the file to delete.
        Raises:
            ValueError: If the file was not found.
            RuntimeError: If the delete operation failed.
        """
        file_index = self.find(path)

        if not self.subsys.fw.call.FileDelete(file_index):
            raise RuntimeError(("FileDelete failed for file \"{}\", "
                                "index {}").format(path, index))

    def rename(self, old, new):
        """Renames a file.

        Args:
            old (str): The path to the file to rename, must already exist.
            new (str): The new name for the file. If the file at this path
                already exists it is overwritten by 'old'.
        Raises:
            ValueError: If the file at path 'old' doesn't exist.
            RuntimeError: If FileRename failed even with valid paths.
        """
        file_index = self.find(old)

        success = self.subsys.fw.call.FileRename(old, len(old),
                                                 new, len(new))
        if not success:
            raise RuntimeError(("FileRename from \"{}\" to "
                                "\"{}\" failed").format(old, new))

    def copy(self, src, dst):
        """Copies a file.

        Args:
            src (str): The path to the file to copy.
            dst (str): The path to copy 'src' to.
        Raises:
            ValueError: If 'src' does not exist
        """
        data = self.read(src)

        # Delete the destination file first so we don't end up appending to it
        try:
            self.delete(dst)
        except ValueError:
            pass

        self.write(dst, data)
