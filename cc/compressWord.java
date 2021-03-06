public static String compressWord(String str) {
     int size = countCompression(str);
     if(size >= str.length()) return str;
     StringBuffer sb = new StringBuffer();
     char last = str.charAt(0);
     int count = 1;
     for(int i = 1; i<str.length(); i++) {
          if(str.charAt(i) == last) count++;
          else {
               sb.append(last).append(count);
               last = str.charAt(i);
               count = 1;
          }
     }
     sb.append(last).append(count);
     return sb.toString();
}

private static int countCompression(String str) {
     if(str == null || str.isEmpty()) return 0;
     char last = str.charAt(0);
     int size = 0;
     int count = 1;
     for(int i = 1; i<str.length(); i++) {
          if(str.charAt(i) == last) count++;
          else {
               last = str.charAt(i);
               size += 1 + String.valueOf(count).length();
               count = 1;
          }
     }
     size += 1 + String.valueOf(count).length();
     return size;
}
