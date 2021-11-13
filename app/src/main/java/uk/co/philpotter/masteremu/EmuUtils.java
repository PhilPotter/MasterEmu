package uk.co.philpotter.masteremu;
/**
This class has some functions used in several parts of this project
 */
public class EmuUtils{
    private static String zerosToDate(int time, int length) {
        String paddedString = String.valueOf(time);
        while (paddedString.length() < length) {
            paddedString = "0" + paddedString;
        }
        return paddedString;
    }
    protected static String fileNameWithDateTime(String startName, String extension){
        Date currentDate = new Date();
        Calendar cal = Calendar.getInstance();
        cal.setTime(currentDate);
        StringBuilder sbFileName = new StringBuilder();
        sbFileName.append(startName);
        sbFileName.append(zerosToDate(cal.get(Calendar.YEAR),4));
        sbFileName.append('-');
        sbFileName.append(zerosToDate(cal.get(Calendar.MONTH) + 1,2));
        sbFileName.append('-');
        sbFileName.append(zerosToDate(cal.get(Calendar.DAY_OF_MONTH),2));
        sbFileName.append('_');
        sbFileName.append(zerosToDate(cal.get(Calendar.HOUR_OF_DAY),2));
        sbFileName.append('-');
        sbFileName.append(zerosToDate(cal.get(Calendar.MINUTE),2));
        sbFileName.append('-');
        sbFileName.append(zerosToDate(cal.get(Calendar.SECOND),2));
        sbFileName.append(extension);
        return sbFileName.toString();
    }
    protected static String fileNameWithDateTime(String extension){
        return fileNameWithDateTime("",extension);
    }
}