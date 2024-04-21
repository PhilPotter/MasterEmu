/* MasterEmu file browser source code file
   copyright Phil Potter, 2024 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.pm.ActivityInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;

import android.content.Intent;

import android.provider.DocumentsContract;
import android.provider.MediaStore;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.CRC32;

/**
 * This class allows the opening of ROM files and save state backups, as well as the saving
 * of save state backups, by making use of the Storage Access Framework built into versions
 * of Android >= API level 19.
 */
public class FileBrowser extends Activity {

    // static variable for passing RomData object to SDL, and checksum to codes activity
    public static RomData transferData = null;
    public static String transferChecksum = null;

    // Request code for file browser request type
    private static final int OPEN_ROM_REQUEST_CODE = 1337;
    private static final int EXPORT_STATES_REQUEST_CODE = 1338;
    private static final int IMPORT_STATES_REQUEST_CODE = 1339;
    private static final int OPEN_ZIP_REQUEST_CODE = 1340;

    // Request string for opening zip rom picker data attached to intent
    public static final String ZIPROMPICKER_URI_ID = "uk.co.philpotter.masteremu.ZipUri";

    // define instance variables
    private String actionType;

    /**
     * This method sets the screen orientation when locked.
     */
    @Override
    protected void onStart() {
        super.onStart();
        if (OptionStore.orientation_lock) {
            if (OptionStore.orientation.equals("portrait")) {
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            } else if (OptionStore.orientation.equals("landscape")) {
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
            }
        } else {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR);
        }
    }

    /**
     * This method uses the SAF and system file picker as required.
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        actionType = getIntent().getStringExtra("actionType");

        // Handle action appropriately
        switch (actionType) {
            case "load_rom":
                Intent openFile = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                openFile.addCategory(Intent.CATEGORY_OPENABLE);
                openFile.setType("*/*");
                startActivityForResult(openFile, OPEN_ROM_REQUEST_CODE);
                break;
            case "export_states":
                Intent createFile = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                createFile.addCategory(Intent.CATEGORY_OPENABLE);
                createFile.setType("application/zip");
                createFile.putExtra(Intent.EXTRA_TITLE,
                    EmuUtils.fileNameWithDateTime("MasterEmu_StateExport_", ".zip"));
                startActivityForResult(createFile, EXPORT_STATES_REQUEST_CODE);
                break;
            case "import_states":
                Intent openStateFile = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                openStateFile.addCategory(Intent.CATEGORY_OPENABLE);
                openStateFile.setType("application/zip");
                startActivityForResult(openStateFile, IMPORT_STATES_REQUEST_CODE);
                break;
        }
    }

    // Put the ROM load routine here to generalise it and allow use by ZIP and non-ZIP
    // ROMs alike
    private void loadRomData(InputStream romStream, String ext) {
        // Open byte array output stream
        ByteArrayOutputStream romBytes = new ByteArrayOutputStream();
        boolean romReadCompleted = false;
        try {
            while (!romReadCompleted) {
                byte[] romSection = new byte[8192];
                int bytesRead = romStream.read(romSection);
                if (bytesRead > 0)
                    romBytes.write(romSection, 0, bytesRead);
                else
                    romReadCompleted = true;
            }
        } catch (IOException e) {
            showMessage("Failed to read ROM file");
            finish();
            return;
        }

        // Get RomData object
        RomData romData = new RomData(romBytes.toByteArray(), ext);
        try {
            romBytes.close();
            romStream.close();
        } catch (IOException e) {
            showMessage("Failed to close ROM file");
            finish();
            return;
        }

        // Now load SDL
        FileBrowser.transferData = romData;

        if (OptionStore.game_genie) {
            // get CRC32 checksum
            CRC32 crcGen = new CRC32();
            crcGen.update(romData.getRomData());
            FileBrowser.transferChecksum = Long.toHexString(crcGen.getValue());
            Intent codesIntent = new Intent(FileBrowser.this, CodesActivity.class);
            startActivity(codesIntent);
        } else {
            Intent sdlIntent = new Intent(FileBrowser.this, SDLActivity.class);
            startActivity(sdlIntent);
        }
        finish();
    }

    /**
     * We use this method as a callback to perform the actual work on the URI
     * returned by the file picker.
     */
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (resultCode) {
            case RESULT_CANCELED:
                switch (requestCode) {
                    case OPEN_ROM_REQUEST_CODE:
                        showMessage("ROM open cancelled");
                        break;
                    case EXPORT_STATES_REQUEST_CODE:
                        showMessage("Export was cancelled");
                        break;
                    case IMPORT_STATES_REQUEST_CODE:
                        showMessage("Import was cancelled");
                        break;
                    case OPEN_ZIP_REQUEST_CODE:
                        // Have this as a handler
                        break;
                }
                finish();
                break;
            case RESULT_OK:
                switch (requestCode) {
                    case OPEN_ZIP_REQUEST_CODE:
                        loadRomData(ZipRomPicker.zis, ZipRomPicker.ext);

                        break;
                    case OPEN_ROM_REQUEST_CODE:
                        Uri romUri = data.getData();
                        String ext = romUri.toString().substring(romUri.toString().lastIndexOf('.')).toLowerCase();
                        if (!(ext.equals(".gg") || ext.equals(".sms") || ext.equals(".zip"))) {
                            ext = getExtensionFromSafUri(romUri);
                        }

                        // Check we support this filetype
                        if (ext == null || !(ext.equals(".gg") || ext.equals(".sms") || ext.equals(".zip"))) {
                            showMessage("Filetype " + ext + " not supported");
                            finish();
                            return;
                        }

                        // Is this a ZIP file?
                        boolean isZip = ext.equals(".zip");

                        // If we selected a ZIP file, we need to get an input stream for
                        // the correct entry
                        if (isZip) {
                            Intent zipRomPicker = new Intent(FileBrowser.this, ZipRomPicker.class);
                            zipRomPicker.putExtra(ZIPROMPICKER_URI_ID, romUri);
                            startActivityForResult(zipRomPicker, OPEN_ZIP_REQUEST_CODE);
                        } else {
                            // Open input stream from ROM file
                            InputStream romStream = null;
                            try {
                                romStream = getContentResolver().openInputStream(romUri);
                            } catch (IOException e) {
                                showMessage("Failed to open ROM file");
                                finish();
                                return;
                            }

                            loadRomData(romStream, ext);
                        }

                        break;
                    case EXPORT_STATES_REQUEST_CODE:
                        Uri stateFileOutUri = data.getData();

                        // Open output stream to state file
                        OutputStream stateFileOutputStream = null;
                        try {
                            stateFileOutputStream = getContentResolver().openOutputStream(stateFileOutUri);
                            StateIO stateManager = new StateIO();
                            boolean result = stateManager.exportToZip(getFilesDir().getAbsolutePath(), stateFileOutputStream);

                            // Handle result
                            if (result) {
                                showMessage("Exported states successfully");
                            } else {
                                showMessage("Failed to export states here");
                                DocumentsContract.deleteDocument(getContentResolver(), stateFileOutUri);
                            }
                        } catch (IOException e) {
                            showMessage("Failed to open state export file for writing");
                            finish();
                            return;
                        }
                        finish();
                        break;
                    case IMPORT_STATES_REQUEST_CODE:
                        Uri stateFileInUri = data.getData();

                        // Open input stream from state file
                        InputStream stateFileInputStream = null;
                        try {
                            stateFileInputStream = getContentResolver().openInputStream(stateFileInUri);
                            StateIO stateManager = new StateIO();
                            boolean result = stateManager.importFromZip(getFilesDir().getAbsolutePath(), stateFileInputStream);

                            // Handle result
                            if (result) {
                                showMessage("Imported states successfully");
                            } else {
                                showMessage("Couldn't find states in this file");
                            }
                        } catch (IOException e) {
                            showMessage("Failed to open state export file for reading");
                            finish();
                            return;
                        }
                        finish();
                        break;
                }
                break;
        }
    }

    /**
     * This shows a message.
     * @param message
     */
    private void showMessage(String message) {
        Toast messageToast = Toast.makeText(this, message, Toast.LENGTH_SHORT);
        messageToast.show();
    }

    /**
     * Get the proper file name from a content-style URI so we can detect its extension.
     */
    private String getExtensionFromSafUri(Uri romUri) {

        // Get content resolver
        ContentResolver romContentResolver = getContentResolver();

        // Query it with the property we want for this URI to get a cursor
        String[] romFileProjection = { MediaStore.MediaColumns.DISPLAY_NAME };
        Cursor romCursor = romContentResolver.query(romUri, romFileProjection, null, null, null);

        // If the cursor was null, crap out here
        if (romCursor == null)
            return null;

        String romFilename = null;
        try {
            if (romCursor.moveToFirst()) {
                romFilename = romCursor.getString(0);
            }
        }
        finally {
            romCursor.close();
        }

        if (romFilename != null) {
            return romFilename.substring(romFilename.toString().lastIndexOf('.')).toLowerCase();
        } else {
            return null;
        }
    }
}
