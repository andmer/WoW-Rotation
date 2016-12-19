## What this does
This is a rotation bot for World of Warcraft that takes a computer vision approach in determining the keys to press. This uses addons in WoW that tell the player what ability to use next such as rotations made with Weak Auras, TellMeWhen, and what I specifically used when making this is an addon called Hekili.

This process of determing which ability to use starts with selecting the region where the next ability to use is displayed in-game. We then use the sum of squared differences template matching technique with our known ability template images and the source image to figure out what ability is being shown on screen.

####Sum of Squared Difference:
![](http://docs.opencv.org/2.4/_images/math/f096a706cb9499736423f10d901c7fe13a1e6926.png)

The template matching had to be quick so I used some ideas from OpenCV by using an integral image for squared pixel values to quickly get the sum of squares. Read more about that [here](http://aishack.in/tutorials/integral-images-opencv/). 

We also needed the sum of products for each position and this by itself is a template matching technique called cross correlation. The math in making this quick to do went a bit beyond me but basically we can use discrete fourier transforms(DFTs) to tranform our source and template images, multiply the points in the frequency domain, and then tranform back to compute the convolutions of the sequences. In doing so we reduce our overall complexity from **O(N^2)** to **O(NlogN)**. [Wikipedia on convolution](https://en.wikipedia.org/wiki/Convolution).

## Setup
To set this up you just have to have an **abilities/** folder with images of the abilites your class has which can be obtained easily from [Wowhead](http://www.wowhead.com/). 

Next, the images should be named using the numerical virtual key code for the keybind the ability has. [List of Virtual Key Codes](http://www.kbdedit.com/manual/low_level_vk_list.html). The name should be the numerical value in that column but in decimal so as an example the key '**Q**' has numerical value **0x51** so we would name the image with that keybind to **81.ext** as that is the decimal representation. 
