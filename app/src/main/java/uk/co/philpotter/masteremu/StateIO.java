/* MasterEmu save state importer and exporter source code file
   copyright Phil Potter, 2022 */

package uk.co.philpotter.masteremu;

import java.io.ByteArrayOutputStream;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.File;
import java.io.FileFilter;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipOutputStream;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipEntry;
import java.util.Arrays;

import android.util.Log;

/**
 * This class defines static methods for exporting save states to and from a zip file.
 */
public class StateIO {

    private static final byte[] headerBuffer = {
        0x43, 0x4D, 0x30, 0x32, 0x5F, 0x42, 0x41, 0x43, 0x4B, 0x55, 0x50
    };

    /**
     * This method attempts to export all save states + folders to a new zip file referenced
     * by the supplied output stream.
     * @param base
     * @param exportFileStream
     */
    public boolean exportToZip(String base, OutputStream exportFileStream) {
        // wrap output stream with a zip output stream
        ZipOutputStream zos = new ZipOutputStream(exportFileStream);

        // create file to represent base dir
        File baseDir = new File(base);

        // get list of savestate directories from base dir
        File[] stateFolders = baseDir.listFiles(new SaveStateDirFilter());
        SaveStateFileFilter stateFilter = new SaveStateFileFilter();

        // write header to backup
        ZipEntry headerEntry = new ZipEntry("backup_header");
        try {
            zos.putNextEntry(headerEntry);
            zos.write(headerBuffer, 0, headerBuffer.length);
            zos.closeEntry();
        }
        catch (Exception e) {
            System.err.println("Couldn't write header file to zip file...");
            errorCleanup(zos);
            return false;
        }

        // iterate through and begin adding files to zip file
        for (int i = 0; i < stateFolders.length; ++i) {
            ZipEntry currentZipDir = new ZipEntry(stateFolders[i].getName() + "/");
            try {
                zos.putNextEntry(currentZipDir);
                zos.closeEntry();
            }
            catch (Exception e) {
                System.err.println("Couldn't create zip entry for directory " + stateFolders[i].getName() + "...");
                errorCleanup(zos);
                return false;
            }

            // iterate through dir, writing savestate files into zip
            String currentDirStr = stateFolders[i].getName() + "/";
            File currentDir = new File(base + "/" + currentDirStr);
            File[] stateFiles = currentDir.listFiles(stateFilter);
            for (int j = 0; j < stateFiles.length; ++j) {
                ZipEntry currentZipStateFile = new ZipEntry(currentDirStr + stateFiles[j].getName());
                try {
                    zos.putNextEntry(currentZipStateFile);

                    FileInputStream currentStateFileStream = new FileInputStream(base + "/" + currentDirStr + stateFiles[j].getName());
                    byte[] stateBuffer = new byte[(int)stateFiles[j].length()];
                    currentStateFileStream.read(stateBuffer);
                    currentStateFileStream.close();
                    zos.write(stateBuffer, 0, stateBuffer.length);

                    zos.closeEntry();
                }
                catch (Exception e) {
                    System.err.println("Couldn't write state file " + stateFiles[j].getName() + " to zip file...");
                    errorCleanup(zos);
                    return false;
                }
            }
        }

        // close zip stream
        try {
            zos.close();
        }
        catch (Exception e) {
            System.err.println("Couldn't close zip file...");
            return false;
        }

        return true;
    }

    /**
     * This method attempts to import all save states + folders from a zip file referenced
     * by the supplied input stream.
     * @param base
     * @param exportFileStream
     */
    public boolean importFromZip(String base, InputStream exportFileStream) {
        // wrap input stream with a zip input stream
        ZipInputStream zis = new ZipInputStream(exportFileStream);

        // iterate through entries of zip file
        ZipEntry current = null;
        boolean firstEntry = true;
        while (true) {
            try {
                current = zis.getNextEntry();
            }
            catch (Exception e) {
                System.err.println("Couldn't get next entry from zip file...");
                closeZipInputStream(zis);
                return false;
            }

            if (current != null) {
                if (firstEntry) {
                    firstEntry = false;

                    // verify header
                    if (!current.getName().equals("backup_header")) {
                        System.err.println("Name of header file incorrect: " + current.getName());
                        closeZipInputStream(zis);
                        return false;
                    } else {
                        byte[] headerFromZipBuffer = new byte[headerBuffer.length];
                        try {
                            zis.read(headerFromZipBuffer, 0, headerBuffer.length);
                        }
                        catch (Exception e) {
                            System.err.println("Couldn't read header file from zip entry...");
                            closeZipInputStream(zis);
                            return false;
                        }

                        if (!Arrays.equals(headerFromZipBuffer, headerBuffer)) {
                            System.err.println("Header file was not correct...");
                            closeZipInputStream(zis);
                            return false;
                        }
                    }
                }
                else {
                    // extract folder or file
                    if (current.isDirectory()) {
                        // make the directory if it doesn't exist
                        File currentDir = new File(base + "/" + current.getName());
                        if (!currentDir.exists()) {
                            currentDir.mkdir();
                        }
                    }
                    else {
                        // delete file if it already exists, and extract from zip file
                        File currentFile = new File(base + "/" + current.getName());
                        if (currentFile.exists())
                            currentFile.delete();

                        // open file output stream
                        FileOutputStream fos = null;
                        try {
                            fos = new FileOutputStream(currentFile);
                        }
                        catch (Exception e) {
                            System.err.println("Couldn't open file output stream to restore " +
                                               currentFile.getName() + "...");
                            closeZipInputStream(zis);
                            return false;
                        }

                        // read file to temp location until we get to end of it
                        ByteArrayOutputStream bos = new ByteArrayOutputStream();
                        byte[] tempBuffer = new byte[128];
                        int bytesRead = 0;
                        try {
                            while ((bytesRead = zis.read(tempBuffer, 0, 128)) != -1) {
                                bos.write(tempBuffer, 0, bytesRead);
                            }
                        }
                        catch (Exception e) {
                            System.err.println("Couldn't read file from zipe file...");
                            closeZipInputStream(zis);
                            return false;
                        }

                        // write file to fos
                        try {
                            fos.write(bos.toByteArray());
                        }
                        catch (Exception e) {
                            System.err.println("Couldn't write file to " + currentFile.getName() + "...");
                            closeZipInputStream(zis);
                            return false;
                        }

                        // close file output stream
                        try {
                            fos.close();
                        }
                        catch (Exception e) {
                            System.err.println("Couldn't close file output stream to " +
                                               currentFile.getName() + "...");
                            closeZipInputStream(zis);
                            return false;
                        }
                    }
                }

                // close entry
                try {
                    zis.closeEntry();
                }
                catch (Exception e) {
                    System.err.println("Couldn't close zip entry...");
                    closeZipInputStream(zis);
                    return false;
                }
            }
            else {
                break;
            }
        }

        // close zip input stream
        closeZipInputStream(zis);
        return true;
    }

    /**
     * This method attempts to delete all save states + folders.
     * @param base
     */
    public boolean deleteAllStates(String base) {
        // create file to represent base dir
        File baseDir = new File(base);

        // get list of savestate directories from base dir
        File[] stateFolders = baseDir.listFiles(new SaveStateDirFilter());
        for (int i = 0; i < stateFolders.length; ++i) {
            removeFolder(stateFolders[i]);
        }

        return true;
    }

    private boolean removeFolder(File folder) {
        File[] temp = folder.listFiles();
        if (temp == null) {
            // we are at lowest level, delete file
            folder.delete();
        }
        else if (temp.length == 0) {
            // directory is empty, delete it
            folder.delete();
        }
        else {
            // recurse into folders
            for (int i = 0; i < temp.length; ++i) {
                removeFolder(temp[i]);
            }
            folder.delete();
        }

        return true;
    }

    private void errorCleanup(ZipOutputStream zos) {
        try {
            zos.close();
        }
        catch (Exception e) {
            System.err.println("Couldn't close ZipOutputStream zos...");
        }
    }

    private void closeZipInputStream(ZipInputStream zis) {
        try {
            zis.close();
        }
        catch (Exception e) {
            System.err.println("Couldn't close ZipInputStream zos...");
        }
    }

    // filters list of items from savestate dir to only give us
    // files we care about
    private class SaveStateFileFilter implements FileFilter {
        @Override
        public boolean accept(File pathname) {
            if (pathname.getName().endsWith(".mesav") || pathname.getName().endsWith(".codes"))
                return true;
            else
                return false;
        }
    }

    // filters list of items from base dir to only give us
    // folders we care about
    private class SaveStateDirFilter implements FileFilter {
        @Override
        public boolean accept(File pathname) {
            if (pathname.isDirectory() && pathname.getName().length() == 8)
                return true;
            else
                return false;
        }
    }
}
