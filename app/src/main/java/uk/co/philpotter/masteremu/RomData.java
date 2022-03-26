/* MasterEmu rom data class source code file
   copyright Phil Potter, 2019 */

package uk.co.philpotter.masteremu;

import android.util.Log;

import java.io.Serializable;
import java.io.BufferedInputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.zip.ZipFile;
import java.util.zip.ZipEntry;

/**
 * This class allows us to abstract the loading of ROM data before passing it to SDL.
 */
public class RomData implements Serializable {

    // instance variables
    private String ext;
    private byte[] romData = null;
    private boolean gg = false;
    private boolean status = false;

    /**
     * This constructor takes a byte array and file extension as input.
     */
    public RomData(byte[] source, String ext) {
        setupRomData(source, ext);
    }

    /**
     * This private method does the actual work.
     */
    private void setupRomData(byte[] source, String ext) {
        // Set file extension and ROM data
        this.ext = ext;
        this.romData = source;
        if (ext.equals(".gg"))
            this.gg = true;

        // check final size
        if (romData.length < 16384 || (romData.length % 16384 != 0)) {
            if (romData.length >= 16384 && (romData.length - 512) % 16384 == 0) {
                int newLength = romData.length - 512;
                byte[] newRomData = new byte[newLength];
                System.arraycopy(romData, 512, newRomData, 0, newLength);
                romData = newRomData;
            } else {
                Log.e("setupRomData:", "ROM is not the correct size");
                return;
            }
        }

        // set ready
        status = true;
    }

    /**
     * This tells us if the RomData object is ready to be used.
     */
    public boolean isReady() {
        return status;
    }

    /**
     * This tells us if the RomData object represents a Game Gear ROM.
     */
    public boolean isGg() {
        return gg;
    }

    /**
     * This returns a reference to the ROM data array.
     */
    public byte[] getRomData() {
        return romData;
    }
}
