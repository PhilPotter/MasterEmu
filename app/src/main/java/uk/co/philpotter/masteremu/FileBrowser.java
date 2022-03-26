/* MasterEmu file browser source code file
   copyright Phil Potter, 2019 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.os.Bundle;

import android.content.Intent;

import android.provider.DocumentsContract;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

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
                }
                finish();
                break;
            case RESULT_OK:
                switch (requestCode) {
                    case OPEN_ROM_REQUEST_CODE:
                        Uri romUri = data.getData();
                        String ext = romUri.toString().substring(romUri.toString().lastIndexOf('.')).toLowerCase();

                        // Check we support this filetype
                        if (!(ext.equals(".gg") || ext.equals(".sms"))) {
                            showMessage("Filetype " + ext + " not supported");
                            finish();
                            return;
                        }

                        // Open input stream from ROM file
                        InputStream romStream = null;
                        try {
                            romStream = getContentResolver().openInputStream(romUri);
                        } catch (IOException e) {
                            showMessage("Failed to open ROM file");
                            finish();
                            return;
                        }

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
                        Intent sdlIntent = new Intent(FileBrowser.this, SDLActivity.class);
                        startActivity(sdlIntent);
                        finish();
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
    public void showMessage(String message) {
        Toast messageToast = Toast.makeText(this, message, Toast.LENGTH_SHORT);
        messageToast.show();
    }
}
