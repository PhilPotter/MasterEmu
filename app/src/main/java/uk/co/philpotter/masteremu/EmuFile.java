/* MasterEmu EmuFile source code file
   copyright Phil Potter, 2019 */

package uk.co.philpotter.masteremu;

import java.io.File;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.net.URI;
import java.io.IOException;
import android.util.Log;

/**
 * This class extends File to allow sorting by attributes.
 */
public class EmuFile extends File {

    private ZipEntry thisFile;
    private ZipFile zipParent;
    private String thisZipPath;
    private boolean isParent;

    // superclass constructors called here
    public EmuFile(EmuFile dir, String name) {
        super(dir, name);
        thisFile = null;
        zipParent = null;
        thisZipPath = null;
        isParent = false;
    }

    public EmuFile(String path) {
        super(path);
        thisFile = null;
        zipParent = null;
        thisZipPath = null;
        isParent = false;
    }

    public EmuFile(String dirPath, String name) {
        super(dirPath, name);
        thisFile = null;
        zipParent = null;
        thisZipPath = null;
        isParent = false;
    }

    public EmuFile(URI uri) {
        super(uri);
        thisFile = null;
        zipParent = null;
        thisZipPath = null;
        isParent = false;
    }

    public EmuFile(ZipEntry ze, ZipFile parent) {
        super(ze.getName());
        thisFile = ze;
        zipParent = parent;
        thisZipPath = ze.getName();
        isParent = false;
    }

    /**
     * This returns the internal zip file reference.
     */
    public ZipFile getZipParent() {
        return zipParent;
    }

    /**
     * This returns the internal zip entry reference.
     */
    public ZipEntry getZipEntry() {
        return thisFile;
    }

    /**
     * This checks if the EmuFile references a zip entry.
     */
    public boolean isZipBased() {
        return !(thisFile == null);
    }

    /**
     * This checks if the EmuFile is a parent reference.
     */
    public boolean isParent() {
        return isParent;
    }

    /**
     * This sets the parent reference.
     */
    public void setParent() {
        isParent = true;
    }

    /**
     * An EmuFile will equal another if they share the same canonical path.
     */
    @Override
    public boolean equals(Object o) {
        boolean returnVal = false;
        try {
            returnVal = ((EmuFile)o).getCanonicalPath().equals(this.getCanonicalPath());
        }
        catch (IOException e) {
            Log.e("EmuFile ERROR:", "Couldn't compare canonical paths");
        }

        return returnVal;
    }

    /**
     * An EmuFile will have the same hashCode as another if they share the same canonical path.
     */
    @Override
    public int hashCode() {
        int returnVal = 0;
        try {
            returnVal = this.getCanonicalPath().hashCode();
        }
        catch (IOException e) {
            Log.e("EmuFile ERROR:", "Couldn't generate hashCode");
        }

        return returnVal;
    }

    /**
     * An EmuFile will be sorted folder first, file next, and alphabetically.
     */
    @Override
    public int compareTo(File f) {
        int returnVal = 0;
        try {
            if (this.isDirectory()) {
                if (f.isDirectory()) {
                    returnVal = this.getCanonicalPath().compareTo(f.getCanonicalPath());
                } else {
                    returnVal = -1;
                }
            } else {
                if (f.isDirectory()) {
                    returnVal = 1;
                } else {
                    returnVal = this.getCanonicalPath().compareTo(f.getCanonicalPath());
                }
            }
        }
        catch (IOException e) {
            Log.e("EmuFile ERROR:", "Couldn't compare files");
        }

        return returnVal;
    }
}
