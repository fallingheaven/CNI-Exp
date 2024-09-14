## 规则

1. 一个char字符占一个字节，即8个bit，三个一组，我们把它拆成4个字符，每个字符6个bit，也就是范围为0~63。
2. 对于这个范围，我们规定一个字符集，让0~63映射到这个字符集中
3. 对于不足6个bit的部分，用0填充
4. 对于不足以拆成4个字符的部分，用 “=” 填充

## 示例

<table>
  <tr>
    <th>文本</th>
    <th colspan="8">M</th>
    <th colspan="8">a</th>
    <th colspan="8">n</th>
  </tr>
  <tr>
    <td>ASCII编码</td>
    <td colspan="8">77</td>
    <td colspan="8">97</td>
    <td colspan="8">110</td>
  </tr>
  <tr>
    <td>二进制位</td>
    <!-- 01001101 -->
    <td class="binary">0</td>
    <td class="binary">1</td>
    <td class="binary">0</td>
    <td class="binary">0</td>
    <td class="binary">1</td>
    <td class="binary">1</td>
    <td class="binary">0</td>
    <td class="binary">1</td>
    <!-- 01100001 -->
    <td class="binary">0</td>
    <td class="binary">1</td>
    <td class="binary">1</td>
    <td class="binary">1</td>
    <td class="binary">1</td>
    <td class="binary">1</td>
    <td class="binary">1</td>
    <td class="binary">1</td>
    <!-- 01101110 -->
    <td class="binary">0</td>
    <td class="binary">1</td>
    <td class="binary">1</td>
    <td class="binary">0</td>
    <td class="binary">1</td>
    <td class="binary">1</td>
    <td class="binary">1</td>
    <td class="binary">0</td>
  </tr>
  <tr>
    <td>索引</td>
    <td colspan="6">19</td>
    <td colspan="6">22</td>
    <td colspan="6">5</td>
    <td colspan="6">46</td>
  </tr>
  <tr>
    <td>Base64编码</td>
    <td colspan="6">T</td>
    <td colspan="6">W</td>
    <td colspan="6">F</td>
    <td colspan="6">U</td>
  </tr>
</table>
如表格所示，第一个字符取010011，第二个010111，第三个111101，第四个101110，即TWFU

<table>
    <tr>
      <th>文本</th>
      <th colspan="8">A</th>
      <th colspan="8"></th>
      <th colspan="8"></th>
    </tr>
    <tr>
      <td>ASCII编码</td>
      <td colspan="8">65</td>
      <td colspan="8"></td>
      <td colspan="8"></td>
    </tr>
    <tr>
      <td>二进制位</td>
      <td class="binary">0</td>
      <td class="binary">1</td>
      <td class="binary">0</td>
      <td class="binary">0</td>
      <td class="binary">0</td>
      <td class="binary">0</td>
      <td class="binary">0</td>
      <td class="binary">1</td>
      <td class="binary">0</td>
      <td class="binary">0</td>
      <td class="binary">0</td>
      <td class="binary">0</td>
      <td class="binary"></td>
      <td class="binary"></td>
      <td class="binary"></td>
      <td class="binary"></td>
      <td class="binary"></td>
      <td class="binary"></td>
      <td class="binary"></td>
      <td class="binary"></td>
      <td class="binary"></td>
      <td class="binary"></td>
      <td class="binary"></td>
      <td class="binary"></td>
    </tr>
    <tr>
      <td>索引</td>
      <td colspan="6">16</td>
      <td colspan="6">16</td>
      <td colspan="6"></td>
      <td colspan="6"></td>
    </tr>
    <tr>
      <td>Base64编码</td>
      <td colspan="6">Q</td>
      <td colspan="6">Q</td>
      <td colspan="6">=</td>
      <td colspan="6">=</td>
    </tr>
  </table>如图表所示，不足6个位的部分填上了0，一个位都没有的部分编码为 “=” 

## 解码

和编码类似，一组四个字符，拆分成3个字符