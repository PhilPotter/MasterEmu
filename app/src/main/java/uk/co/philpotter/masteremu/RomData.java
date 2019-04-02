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
    private byte[] romData = null;
    private boolean gg = false;
    private boolean status = false;

    /**
     * This constructor takes an EmuFile as input and loads ROM data from it.
     */
    public RomData(EmuFile source) {
        setupRomData(source);
    }

    /**
     * This private method does the actual work.
     */
    private void setupRomData(EmuFile source) {
        if (source.isZipBased()) { // zip based, load from zip file

            // get parent zip file and entry
            ZipFile parent = source.getZipParent();
            ZipEntry entry = source.getZipEntry();

            // setup input stream
            BufferedInputStream romReader = null;
            try {
                romReader = new BufferedInputStream(parent.getInputStream(entry));
            }
            catch (IOException e) {
                Log.e("setupRomData:", "Couldn't get ROM input stream from zip file");
                return;
            }

            // setup destination buffer
            if (entry.getSize() == -1L) {
                Log.e("setupRomData:", "Couldn't get ROM size from zip file");
                return;
            }
            romData = new byte[(int)entry.getSize()];

            // read compressed file into buffer
            try {
                romReader.read(romData, 0, (int)entry.getSize());
            }
            catch (IOException e) {
                Log.e("setupRomData:", "Couldn't get read ROM from zip file");
                return;
            }

            // close stream
            try {
                romReader.close();
            }
            catch (IOException e) {
                Log.e("setupRomData:", "Couldn't close zip file input stream");
                return;
            }

            // set gg status if necessary
            if (entry.getName().toLowerCase().endsWith(".gg")) {
                gg = true;
            }
        }
        else { // not zip based, load as normal

            // setup input stream
            BufferedInputStream romReader = null;
            try {
                romReader = new BufferedInputStream(new FileInputStream(source));
            }
            catch (IOException e) {
                Log.e("setupRomData:", "Couldn't get ROM input stream from zip file");
                return;
            }

            // setup destination buffer
            if (source.length() == 0L) {
                Log.e("setupRomData:", "Couldn't get ROM size");
                return;
            }
            romData = new byte[(int)source.length()];

            // read file into buffer
            try {
                romReader.read(romData, 0, (int)source.length());
            }
            catch (IOException e) {
                Log.e("setupRomData:", "Couldn't get read ROM from file");
                return;
            }

            // close stream
            try {
                romReader.close();
            }
            catch (IOException e) {
                Log.e("setupRomData:", "Couldn't close file input stream");
                return;
            }

            // set gg status if necessary
            if (source.getName().toLowerCase().endsWith(".gg")) {
                gg = true;
            }
        }

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
